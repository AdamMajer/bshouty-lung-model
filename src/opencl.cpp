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

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <QLibrary>
#include <QMessageBox>
#include "opencl.h"
#include <stdexcept>


OpenCL *cl;

static void* allocPageAligned(int bytes)
{
	char *ptr = (char*)malloc(bytes + 4096 + sizeof(void*));

	if (ptr == 0)
		throw opencl_exception(QLatin1String("Out of memory!"));

	void **p = (void**)((quint64)(ptr + 4096 + sizeof(void*)) & ~(4096L-1ULL));
	*(p-1) = ptr;

	return p;
}

static void freePageAligned(void *ptr)
{
	void **p = (void**)ptr;
	free(*(p-1));
}



OpenCL::OpenCL()
{
	/* Only use OpenCL on 64-bit platforms. 32-bit platforms are obsolete and
	 * do not have latest hardware these days
	 */
	n_devices = 0;

#if (QT_POINTER_SIZE == 8)
	resolveFunctions();
#else
	memset(&opencl, 0, sizeof(opencl));
#endif

	if (opencl.clGetPlatformIDs == NULL)
		return;

	QFile program_in(qApp->applicationDirPath() + "/vessel_integration.cl");
	if (!program_in.open(QIODevice::ReadOnly)) {
		memset(&opencl, 0, sizeof(opencl));
		return;
	}
	program_src = program_in.readAll();
	program_in.close();

	try {
		cl_platform_id platform_ids[5];
		cl_uint platform_nums;
		errorCheck(opencl.clGetPlatformIDs(5, platform_ids, &platform_nums));

		for (cl_uint i=0; i<platform_nums; ++i)
			addPlatform(platform_ids[i]);
	}
	catch (opencl_exception e) {
		QString msg = QString("OpenCL Disabled due to following error:\n\n%1").arg(e.what());
		QMessageBox::information(0, "OpenCL Error", msg );
		memset(&opencl, 0, sizeof(opencl));
		n_devices = 0;
	}
}

OpenCL::~OpenCL()
{
	foreach (const OpenCL_device &d, opencl_devices) {
		opencl.clReleaseMemObject(d.mem_results);
		opencl.clReleaseMemObject(d.mem_vein_buffer);

		opencl.clReleaseCommandQueue(d.queue);
		opencl.clReleaseProgram(d.program);
		opencl.clReleaseContext(d.context);

		freePageAligned(d.cl_vessel);
		freePageAligned(d.integration_workspace);
	}
}

bool OpenCL::isAvailable() const
{
	return opencl.clGetPlatformIDs != NULL;
}

OpenCLDeviceList OpenCL::devices() const
{
	return opencl_devices;
}

OpenCL_func OpenCL::functions() const
{
	return opencl;
}

void OpenCL::errorCheck(cl_int status) throw(opencl_exception)
{
	if (status == CL_SUCCESS)
		return;

	throw opencl_exception(QString("OpenCL Error: %1").arg(status));
}

void OpenCL::resolveFunctions()
{
	QLibrary lib("OpenCL");

	if (!lib.load()){
		memset(&opencl, 0, sizeof(opencl));
		return;
	}

#define RESOLVE_CL(a) \
	if ((opencl.a = (a##_ptr)lib.resolve(#a)) == NULL) \
	    goto resolveOpenCL_load_error

	RESOLVE_CL(clGetPlatformIDs);
	RESOLVE_CL(clGetPlatformInfo);
	RESOLVE_CL(clGetDeviceIDs);
	RESOLVE_CL(clGetDeviceInfo);
	RESOLVE_CL(clCreateContext);
	RESOLVE_CL(clReleaseContext);
	RESOLVE_CL(clCreateCommandQueue);
	RESOLVE_CL(clReleaseCommandQueue);
	RESOLVE_CL(clCreateBuffer);
	RESOLVE_CL(clEnqueueReadBuffer);
	RESOLVE_CL(clEnqueueWriteBuffer);
	RESOLVE_CL(clReleaseMemObject);
	RESOLVE_CL(clCreateProgramWithSource);
	RESOLVE_CL(clGetProgramBuildInfo);
	RESOLVE_CL(clReleaseProgram);
	RESOLVE_CL(clBuildProgram);
	RESOLVE_CL(clCreateKernel);
	RESOLVE_CL(clSetKernelArg);
	RESOLVE_CL(clEnqueueNDRangeKernel);

#undef RESOLVE_CL

	return; // all functions resolved

resolveOpenCL_load_error:
	QMessageBox::information(0, "OpenCL error", "OpenCL function resolve error");
	memset(&opencl, 0, sizeof(opencl));
}

void OpenCL::addPlatform(cl_platform_id platform_id)
{
	cl_device_id devices[16];
	cl_uint num_devices;

	errorCheck(opencl.clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ALL,
	                                 16, devices, &num_devices));
	for (cl_uint i=0; i<num_devices; ++i)
		addDevice(platform_id, devices[i]);
}

void OpenCL::addDevice(cl_platform_id platform_id, cl_device_id device_id)
{
	OpenCL_device dev;

	dev.platform_id = platform_id;
	dev.device_id = device_id;

	errorCheck(opencl.clGetDeviceInfo(device_id, CL_DEVICE_TYPE,
	                                  sizeof(cl_device_type),
	                                  &dev.device_type, 0));

	switch (dev.device_type) {
	case CL_DEVICE_TYPE_CPU:
	case CL_DEVICE_TYPE_GPU:
	case CL_DEVICE_TYPE_ACCELERATOR:
		break;
	default:
		// unknown device, ignored
		return;
	}

	cl_uint max_dims;
	errorCheck(opencl.clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS,
	                                  sizeof(cl_uint),
	                                  &max_dims, 0));

	size_t *max_work_sizes = new size_t[max_dims];
	errorCheck(opencl.clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_ITEM_SIZES,
	                                  sizeof(size_t)*max_dims,
	                                  max_work_sizes, 0));
	dev.max_work_item_size[0] = max_work_sizes[0];
	dev.max_work_item_size[1] = max_work_sizes[1];

	errorCheck(opencl.clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_GROUP_SIZE,
	                                  sizeof(size_t),
	                                  &dev.max_work_group_size, 0));

	errorCheck(opencl.clGetDeviceInfo(device_id, CL_DEVICE_GLOBAL_MEM_SIZE,
	                                  sizeof(cl_ulong),
	                                  &dev.max_mem_size, 0));
	errorCheck(opencl.clGetDeviceInfo(device_id, CL_DEVICE_MAX_MEM_ALLOC_SIZE,
	                                  sizeof(cl_ulong),
	                                  &dev.max_alloc_size, 0));
	errorCheck(opencl.clGetDeviceInfo(device_id, CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE,
	                                  sizeof(cl_uint),
	                                  &dev.cacheline_size, 0));

	if (dev.cacheline_size > 256) {
		QMessageBox::information(0, "OpenCL optimization error",
		                         QString("Cacheline padding on OpenCL device is %1 which is larger than\n"
		                                 "maximum cacheline size configured in this application. Please\n"
		                                 "adjust the model source code for larger cacheline padding!").arg(dev.cacheline_size));
	}

	cl_bool cl_compiler_available;
	errorCheck(opencl.clGetDeviceInfo(device_id, CL_DEVICE_COMPILER_AVAILABLE,
	                                  sizeof(cl_bool),
	                                  &cl_compiler_available, 0));
	if (!cl_compiler_available)
		return;

	cl_bool cl_device_available;
	errorCheck(opencl.clGetDeviceInfo(device_id, CL_DEVICE_AVAILABLE,
	                                  sizeof(cl_bool),
	                                  &cl_device_available, 0));
	if (!cl_device_available)
		return;

	errorCheck(opencl.clGetDeviceInfo(device_id, CL_DEVICE_NAME,
	                                  sizeof(char)*512,
	                                  &dev.name, NULL));

	n_devices++;

	QCryptographicHash hash(QCryptographicHash::Md5);
	hash.addData(dev.name, strlen(dev.name));
	hash.addData((const char*)&dev.device_type, sizeof(dev.device_type));
	hash.addData((const char*)&n_devices, sizeof(int));
	dev.hash = hash.result();
	dev.device_enabled = true;

	cl_int err;
	cl_context_properties properties[] = { CL_CONTEXT_PLATFORM,
	                                       (cl_context_properties)platform_id,
	                                       0 };
	dev.context = opencl.clCreateContext(properties, 1, &device_id, NULL, NULL, &err);
	errorCheck(err);

	const char *prog_ptr = program_src.constData();
	dev.program = opencl.clCreateProgramWithSource(dev.context, 1, &prog_ptr, 0, &err);
	errorCheck(err);

	err = opencl.clBuildProgram(dev.program, 1, &device_id, "-cl-single-precision-constant", NULL, NULL);

	char msg[10240];
	memset(msg, 0, 10240);
	cl_build_status stat;
	size_t len;
	opencl.clGetProgramBuildInfo(dev.program, device_id, CL_PROGRAM_BUILD_STATUS, sizeof(stat), &stat, 0);
	opencl.clGetProgramBuildInfo(dev.program, device_id, CL_PROGRAM_BUILD_LOG, 10240, msg, &len);

	qDebug() << "Compiler result: " << stat;
	qDebug() << msg;

	if (err != CL_SUCCESS) {
		if (len < 10240)
			throw opencl_exception(QString::fromLocal8Bit(msg));
		else
			throw opencl_exception(QString::fromLatin1("Compilation error"));
	}
	errorCheck(err);

	dev.queue = opencl.clCreateCommandQueue(dev.context, device_id, 0, &err);
	errorCheck(err);

	dev.segmentedVesselKernel = opencl.clCreateKernel(dev.program, "segmentedVesselFlow", &err);
	errorCheck(err);
	dev.rigidVesselKernel = opencl.clCreateKernel(dev.program, "rigidVesselFlow", &err);
	errorCheck(err);

	dev.mem_vein_buffer = opencl.clCreateBuffer(dev.context, CL_MEM_READ_ONLY, sizeof(CL_Vessel)*4*256*100, NULL, &err);
	cl->errorCheck(err);
	dev.mem_results = opencl.clCreateBuffer(dev.context, CL_MEM_WRITE_ONLY, sizeof(CL_Result)*4*256*100, NULL, &err);
	cl->errorCheck(err);

	dev.integration_workspace = (CL_Result*)allocPageAligned(sizeof(CL_Result)*102400);
	dev.cl_vessel = (CL_Vessel*)allocPageAligned(sizeof(CL_Vessel)*102400);

	opencl_devices.push_back(dev);
}
