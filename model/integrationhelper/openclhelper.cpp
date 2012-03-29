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
	bool uses_vein_pressures;

	int *vessel_idx;

	WorkGroup(int ne, cl_kernel k, Vessel *v, bool b, int *i)
	        :n_elements(ne), kernel(k), vessels(v), uses_vein_pressures(b), vessel_idx(i) {}
};

OpenCLIntegrationHelper::OpenCLIntegrationHelper(Model *model)
        : AbstractIntegrationHelper(model), n_devices(cl->nDevices())
{
	has_errors = false;
	is_available = cl->isAvailable();
	inside_vein_pressure = new float[nElements()];
	if (inside_vein_pressure == 0)
		throw std::bad_alloc();

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
	if (inside_vein_pressure)
		delete[] inside_vein_pressure;
}

double OpenCLIntegrationHelper::integrate()
{

	/*
	 */
	QFutureSynchronizer<float> futures;

	n_elements = nElements();
	inside_vein_pressure[0] = LAP();
	for (int i=1; i<n_elements; ++i)
		inside_vein_pressure[i] = veins()[(i-1)/2].pressure;

	art_index = 0;
	vein_index = 0;

	for (int i=0; i<n_devices; ++i) {
		QFuture<float> f = QtConcurrent::run(this,
		                          &OpenCLIntegrationHelper::integrateByDevice,
		                          d[i]);
		futures.addFuture(f);
	}

	QList<QFuture<float> > results = futures.futures();
	float ret = 0.0;
	foreach (const QFuture<float> &r, results)
		ret = qMax(ret, r.result());

	return ret;
}

float OpenCLIntegrationHelper::integrateByDevice(OpenCL_device &dev)
{
	const int n_outside_elements = nOutsideElements();

	struct CL_Vessel *cl_vessel = dev.cl_vessel;
	float *ret_values = dev.integration_workspace;
	float ret = 0.0;

	/* Integrate all arteries, then integrate veins. Scheduling same code in the work
	 * unit maximizes throuput and efficiency.
	 * Compute items in blocks of 1024 elements, using blocks of 4x256,
	 * or number of elemnets in a given block, whichever is smaller
	 */

	const struct WorkGroup group[4] = {
	        WorkGroup(n_outside_elements, dev.intOutsideArtery, arteries(), false, &art_index),
	        WorkGroup(n_elements,         dev.intInsideArtery,  arteries(), false, &art_index),
	        WorkGroup(n_outside_elements, dev.intOutsideVein,   veins(),    true,  &vein_index),
	        WorkGroup(n_elements,         dev.intInsideVein,    veins(),    true,  &vein_index),
	};

	try {
		for (int i=0; i<4; ++i) {
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
        float *ret_values_buf)
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
		err = f.clSetKernelArg(w.kernel, kernel_arg_no++, sizeof(cl_mem), &dev.mem_vein_buffer);
		cl->errorCheck(err);
		if (w.uses_vein_pressures) {
			err = f.clEnqueueWriteBuffer(dev.queue, dev.mem_pressures, CL_FALSE,
			                             0, sizeof(float)*n,
			                             inside_vein_pressure+idx,
			                             0, NULL, NULL);
			cl->errorCheck(err);
			err = f.clSetKernelArg(w.kernel, kernel_arg_no++, sizeof(cl_mem), &dev.mem_pressures);
			cl->errorCheck(err);
		}
		err = f.clSetKernelArg(w.kernel, kernel_arg_no++, sizeof(cl_mem), &dev.mem_results);
		cl->errorCheck(err);

		size_t dims[2];
		dims[0] = 4;
		dims[1] = (n+3)/4; // only need 1 "extra" dimention if n is not power of 4
		err = f.clEnqueueNDRangeKernel(dev.queue, w.kernel, 2, NULL, dims, NULL, 0, NULL, NULL);
		cl->errorCheck(err);

		err = f.clEnqueueReadBuffer(dev.queue, dev.mem_results, CL_TRUE,
		                            0, sizeof(float)*n, ret_values_buf,
		                            0, NULL, NULL);
		cl->errorCheck(err);

		for (int i=0; i<n; ++i) {
			w.vessels[idx+i].R = ret_values_buf[i];
			ret = qMax(ret, (cl_vessel_buf[i].R - ret_values_buf[i])/cl_vessel_buf[i].R);
		}

		// qDebug() << "dev type: " << (int)dev.device_type << "  ret: " << ret;

		vessel_index_mutex.lock();
	}
	vessel_index_mutex.unlock();

	return ret;
}

void OpenCLIntegrationHelper::assignVessels(CL_Vessel *cl_vessels, const Vessel *vessels, int n)
{
	for (int i=0; i<n; ++i) {
		const Vessel &v = vessels[i];
		CL_Vessel &c = cl_vessels[i];

		c.a = v.a;
		c.b = v.b;
		c.c = v.c;
		c.flow = v.flow;
		c.GP = v.GP;
		c.Kz = v.Kz;
		c.peri_a = v.perivascular_press_a;
		c.peri_b = v.perivascular_press_b;
		c.peri_c = v.perivascular_press_c;
		c.Ppl = v.Ppl;
		c.pressure = v.pressure;
		c.R = v.R;
		c.tone = v.tone;
	}
}
