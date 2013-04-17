/*
 *   Bshouty Lung Model - Pulmonary Circulation Simulation
 *    Copyright (c) 1989-2012 Zoheir Bshouty, MD, PhD, FRCPC
 *    Copyright (c) 2011-2012 Adam Majer
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "openclhelper.h"
#include "cpuhelper.h"
#include <QtConcurrentRun>
#include <QFutureSynchronizer>
#include <QDebug>
#include <QFile>
#include <limits>

struct WorkGroup {
	int n_elements;
	cl_kernel kernel;
	Vessel *vessels;

	QAtomicInt &vessel_idx;

	WorkGroup(int ne, cl_kernel k, Vessel *v, QAtomicInt &i)
	        :n_elements(ne), kernel(k), vessels(v), vessel_idx(i) {}
};

OpenCLIntegrationHelper::OpenCLIntegrationHelper(Model *model, Model::IntegralType type)
        : AbstractIntegrationHelper(model, type),
          n_devices(cl->nDevices())
{
	error = 0;
	is_available = cl->isAvailable();

	if (!is_available)
		return;

	OpenCLDeviceList devices = cl->devices();
	if (devices.empty())
		return;

	try {
		d.reserve(n_devices);

		for (OpenCLDeviceList::const_iterator i=devices.begin(); i!=devices.end(); ++i)
			d.push_back(*i);
	}
	catch (opencl_exception e) {
		is_available = false;
	}

	cpu_helper = new CpuIntegrationHelper(model, Model::SegmentedVesselFlow);
}

OpenCLIntegrationHelper::~OpenCLIntegrationHelper()
{
	delete cpu_helper;
}

double OpenCLIntegrationHelper::multiSegmentedVessels()
{

	/*
	 */
	QFutureSynchronizer<float> futures;

	art_index = 0;
	vein_index = 0;

	for (int i=0; i<n_devices; ++i) {
		futures.addFuture(QtConcurrent::run(this,
		        &OpenCLIntegrationHelper::integrateByDevice,
		        d[i], d[i].multiSegmentedVesselKernel));
	}

	QList<QFuture<float> > results = futures.futures();
	float ret = 0.0;
	foreach (const QFuture<float> &r, results)
		ret = qMax(ret, r.result());

	return ret;
}

double OpenCLIntegrationHelper::singleSegmentVessels()
{
	QFutureSynchronizer<float> futures;

	art_index = 0;
	vein_index = 0;

	for (int i=0; i<n_devices; ++i) {
		futures.addFuture(QtConcurrent::run(this,
		        &OpenCLIntegrationHelper::integrateByDevice,
		        d[i], d[i].singleSegmentVesselKernel));
	}

	QList<QFuture<float> > results = futures.futures();
	float ret = 0.0;
	foreach (const QFuture<float> &r, results)
		ret = qMax(ret, r.result());

	return ret;
}

double OpenCLIntegrationHelper::capillaryResistances()
{
	return cpu_helper->capillaryResistances();
}

float OpenCLIntegrationHelper::integrateByDevice(OpenCL_device &dev, cl_kernel k)
{
	struct CL_Vessel *cl_vessel = dev.cl_vessel;
	struct CL_Result *ret_values = dev.integration_workspace;
	float ret = 0.0;

	/* Integrate all arteries, then integrate veins. Scheduling same code in the work
	 * unit maximizes throuput and efficiency.
	 * Compute items in blocks of 1024 elements, using blocks of 4x256,
	 * or number of elemnets in a given block, whichever is smaller
	 */

	const struct WorkGroup group[2] = {
	        WorkGroup(nArteries(), k, arteries(), art_index),
	        WorkGroup(nVeins(),    k, veins(),    vein_index)
	};

	try {
		for (int i=0; i<2; ++i) {
			float v = processWorkGroup(group[i], dev, cl_vessel, ret_values);
			ret = qMax(v, ret);
		}
	}
	catch (opencl_exception e) {
		qDebug() << "OpenCL ERROR: " << e.what();
		error = e.error_no;
	}

	return ret;
}

float OpenCLIntegrationHelper::processWorkGroup(
        const WorkGroup &w,
        OpenCL_device &dev,
        CL_Vessel *cl_vessel_buf,
        CL_Result *ret_values_buf)
{
	const OpenCL_func f = cl->functions();
	cl_int err;
	float ret = 0.0;

	const int elements_per_function = (dev.device_type == CL_DEVICE_TYPE_GPU) ? 102400 : 1024;

	/* Careful with locks. The lock is locked before and at end of loop!. */
	const int max_n = std::min(dev.max_work_item_size[0]*dev.max_work_item_size[1],
	                static_cast<size_t>(elements_per_function));
	int idx;
	while ((idx = w.vessel_idx.fetchAndAddOrdered(max_n)) < w.n_elements) {

		int n = max_n;
		if (idx+max_n > w.n_elements)
			n = w.n_elements-idx;

		size_t real_vessels = assignVessels(cl_vessel_buf,
		                                    w.vessels+idx,
		                                    n,
		                                    dev.max_work_item_size[0]);

		if (real_vessels <= 0)
			continue;

		err = f.clEnqueueWriteBuffer(dev.queue, dev.mem_vein_buffer, CL_FALSE,
		                             0, sizeof(CL_Vessel)*real_vessels, cl_vessel_buf,
		                             0, NULL, NULL);
		cl->errorCheck(err, __FUNCTION__, __LINE__);
		int kernel_arg_no = 0;

		cl_int width = std::min(dev.max_work_item_size[0], real_vessels);
		err = f.clSetKernelArg(w.kernel, kernel_arg_no++, sizeof(cl_int), &width);
		cl->errorCheck(err, __FUNCTION__, __LINE__);

		cl_float hct = Hct();
		err = f.clSetKernelArg(w.kernel, kernel_arg_no++, sizeof(cl_float), &hct);
		cl->errorCheck(err, __FUNCTION__, __LINE__);

		cl_float tlrns = Tlrns();
		err = f.clSetKernelArg(w.kernel, kernel_arg_no++, sizeof(cl_float), &tlrns);
		cl->errorCheck(err, __FUNCTION__, __LINE__);

		err = f.clSetKernelArg(w.kernel, kernel_arg_no++, sizeof(cl_mem), &dev.mem_vein_buffer);
		cl->errorCheck(err, __FUNCTION__, __LINE__);

		err = f.clSetKernelArg(w.kernel, kernel_arg_no++, sizeof(cl_mem), &dev.mem_results);
		cl->errorCheck(err, __FUNCTION__, __LINE__);

		size_t dims[2];
		dims[0] = std::min(dev.max_work_item_size[0], real_vessels);
		dims[1] = real_vessels/dims[0];
		if (real_vessels%dims[0] > 0)
			dims[1]++;

		err = f.clEnqueueNDRangeKernel(dev.queue, w.kernel, 2, NULL, dims, NULL, 0, NULL, NULL);
		cl->errorCheck(err, __FUNCTION__, __LINE__);

		err = f.clEnqueueReadBuffer(dev.queue, dev.mem_results, CL_TRUE,
		                            0, sizeof(CL_Result)*real_vessels, ret_values_buf,
		                            0, NULL, NULL);
		cl->errorCheck(err, __FUNCTION__, __LINE__);

		updateResults(ret_values_buf, w.vessels+idx, n);
		for (size_t i=0; i<real_vessels; ++i) {
			if (isinf(ret_values_buf[i].R) || isnan(ret_values_buf[i].R)) {
				qDebug("INF: %d", static_cast<int>(idx+i));
			}
			ret = qMax(ret, ret_values_buf[i].delta_R);
		}
	}

	return ret;
}

int OpenCLIntegrationHelper::assignVessels(CL_Vessel *cl_vessels,
                                           const Vessel *vessels,
                                           int n,
                                           int section_size)
{
	// returns actual number to be executed
	int total_vessels = 0;
	for (int i=0; i<n; ++i) {
		const Vessel &v = vessels[i];
		CL_Vessel &c = cl_vessels[total_vessels];

		if (v.flow <= 1e-50 ||
		    isnan(v.pressure_in) ||
		    isnan(v.pressure_out)/* ||
		    v.pressure_out < 0.0*/) {

			continue;
		}

		c.max_a = v.max_a;
		c.gamma = v.gamma;
		c.phi = v.phi;
		c.c = v.c;
		c.flow = v.flow;
		c.GP = v.GP;
		c.peri_a = v.perivascular_press_a;
		c.peri_b = v.perivascular_press_b;
		c.peri_c = v.perivascular_press_c;
		c.Ppl = v.Ppl;
		c.pressure_in = v.pressure_in;
		c.pressure_out = v.pressure_out;
		c.R = v.R;
		c.tone = v.tone;

		c.D = v.D;
		c.len = v.length;

		c.vessel_ratio = v.vessel_ratio;

		total_vessels++;
	}

	// zero remainieg vessels in the section
	int remaining = n % section_size;
	if (remaining > 0) {
		remaining = section_size - remaining;
		memset(cl_vessels+total_vessels, 0, remaining*sizeof(CL_Vessel));
	}

	return total_vessels;
}

void OpenCLIntegrationHelper::updateResults(const CL_Result *cl_vessels,
                                            Vessel *vessels,
                                            int n)
{
	int idx = 0;
	for (int i=0; i<n; ++i) {
		Vessel &v = vessels[i];
		const CL_Result &r = cl_vessels[idx];

		if (v.flow <= 1e-50 || isnan(v.pressure_in) || isnan(v.pressure_out))
			continue;

		if (r.D < 0.1 /*|| v.pressure_out < 0.0*/) {
			v.viscosity_factor = v.R = std::numeric_limits<double>::infinity();
			v.Dmax = 0.0;
			v.Dmin = 0.0;
			v.D_calc = 0.0;
			v.volume = 0.0;
			v.last_delta_R = 0.0;
			continue;
		}

		if (!isnan(r.D)) {
			v.R = r.R;
			v.last_delta_R = r.delta_R;
			v.D_calc = r.D;
			v.Dmax = r.Dmax;
			v.Dmin = r.Dmin;
			v.viscosity_factor = r.viscosity_factor;
			v.volume = r.vol;
		}
		else
			qDebug() << "+ NaN";

		idx++;
	}
}
