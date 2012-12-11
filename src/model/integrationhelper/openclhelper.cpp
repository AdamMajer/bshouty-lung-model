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
#include <QtConcurrentRun>
#include <QFutureSynchronizer>
#include <QDebug>
#include <QFile>


struct WorkGroup {
	int n_elements;
	cl_kernel kernel;
	Vessel *vessels;

	int *vessel_idx;

	WorkGroup(int ne, cl_kernel k, Vessel *v, int *i)
	        :n_elements(ne), kernel(k), vessels(v), vessel_idx(i) {}
};

OpenCLIntegrationHelper::OpenCLIntegrationHelper(Model *model)
        : AbstractIntegrationHelper(model, Model::BshoutyIntegral),
          n_devices(cl->nDevices())
{
	has_errors = false;
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
}

OpenCLIntegrationHelper::~OpenCLIntegrationHelper()
{

}

double OpenCLIntegrationHelper::integrateBshoutyModel()
{

	/*
	 */
	QFutureSynchronizer<float> futures;

	n_elements = nElements();
	art_index = 0;
	vein_index = 0;

	for (int i=0; i<n_devices; ++i) {
		futures.addFuture(QtConcurrent::run(this,
		        &OpenCLIntegrationHelper::integrateByDevice,
		        d[i], d[i].intVessel));
	}

	QList<QFuture<float> > results = futures.futures();
	float ret = 0.0;
	foreach (const QFuture<float> &r, results)
		ret = qMax(ret, r.result());

	return ret;
}

double OpenCLIntegrationHelper::laminarFlow()
{
	QFutureSynchronizer<float> futures;

	n_elements = nElements();
	art_index = 0;
	vein_index = 0;

	for (int i=0; i<n_devices; ++i) {
		futures.addFuture(QtConcurrent::run(this,
		        &OpenCLIntegrationHelper::integrateByDevice,
		        d[i], d[i].rigidFlowVessel));
	}

	QList<QFuture<float> > results = futures.futures();
	float ret = 0.0;
	foreach (const QFuture<float> &r, results)
		ret = qMax(ret, r.result());

	return ret;
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
	        WorkGroup(n_elements, k, arteries(), &art_index),
	        WorkGroup(n_elements, k, veins(),    &vein_index)
	};

	try {
		for (int i=0; i<2; ++i) {
			float v = processWorkGroup(group[i], dev, cl_vessel, ret_values);
			ret = qMax(v, ret);
		}
	}
	catch (opencl_exception e) {
		qDebug() << "OpenCL ERROR: " << e.what();
		has_errors = true;
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

	/* Careful with locks. The lock is locked before and at end of loop!. */
	vessel_index_mutex.lock();
	while (*w.vessel_idx < w.n_elements) {
		int idx = *w.vessel_idx;
		int n;
		n = (w.n_elements-idx > 1024) ? 1024 : w.n_elements-idx;
		*w.vessel_idx += n;
		vessel_index_mutex.unlock();

		assignVessels(cl_vessel_buf, w.vessels+idx, n);
		err = f.clEnqueueWriteBuffer(dev.queue, dev.mem_vein_buffer, CL_FALSE,
		                             0, sizeof(CL_Vessel)*n, cl_vessel_buf,
		                             0, NULL, NULL);
		cl->errorCheck(err);
		int kernel_arg_no = 0;

		cl_float hct = Hct();
		err = f.clSetKernelArg(w.kernel, kernel_arg_no++, sizeof(float), &hct);
		cl->errorCheck(err);

		err = f.clSetKernelArg(w.kernel, kernel_arg_no++, sizeof(cl_mem), &dev.mem_vein_buffer);
		cl->errorCheck(err);

		err = f.clSetKernelArg(w.kernel, kernel_arg_no++, sizeof(cl_mem), &dev.mem_results);
		cl->errorCheck(err);

		size_t dims[2];
		dims[0] = 32;
		dims[1] = (n+31)/32; // only need 1 "extra" dimention if n is not a multiple of 32
		err = f.clEnqueueNDRangeKernel(dev.queue, w.kernel, 2, NULL, dims, NULL, 0, NULL, NULL);
		cl->errorCheck(err);

		err = f.clEnqueueReadBuffer(dev.queue, dev.mem_results, CL_TRUE,
		                            0, sizeof(CL_Result)*n, ret_values_buf,
		                            0, NULL, NULL);
		cl->errorCheck(err);

		updateResults(ret_values_buf, w.vessels+idx, n);
		for (int i=0; i<n; ++i) {
			if (isinf(ret_values_buf[i].R) || isnan(ret_values_buf[i].R)) {
				qDebug("INF");
			}
			ret = qMax(ret, ret_values_buf[i].delta_R);
		}

		// qDebug() << "dev type: " << (int)dev.device_type << "  ret: " << ret;

		vessel_index_mutex.lock();
	}
	vessel_index_mutex.unlock();

	return ret;
}

void OpenCLIntegrationHelper::assignVessels(CL_Vessel *cl_vessels,
                                            const Vessel *vessels,
                                            int n)
{
	for (int i=0; i<n; ++i) {
		const Vessel &v = vessels[i];
		CL_Vessel &c = cl_vessels[i];

		c.max_a = v.max_a;
		c.a = v.a;
		c.b = v.b;
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
	}
}

void OpenCLIntegrationHelper::updateResults(const CL_Result *cl_vessels,
                                            Vessel *vessels,
                                            int n)
{
	for (int i=0; i<n; ++i) {
		Vessel &v = vessels[i];
		const CL_Result &r = cl_vessels[i];

		v.R = r.R;
		v.last_delta_R = r.delta_R;
		v.D_calc = r.D;
		v.Dmax = r.Dmax;
		v.Dmin = r.Dmin;
		v.viscosity_factor = r.viscosity_factor;
		v.volume = r.vol;
	}
}
