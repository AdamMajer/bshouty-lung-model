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

#include "common.h"
#include <QByteArray>
#include <CL/opencl.h>
#include <QString>
#include <stdexcept>
#include <list>

typedef cl_int (*clGetPlatformIDs_ptr)(cl_uint, cl_platform_id *, cl_uint *);
typedef cl_int (*clGetPlatformInfo_ptr)(cl_platform_id, cl_platform_info,
                                        size_t, void *, size_t *);

typedef cl_int (*clGetDeviceIDs_ptr)(cl_platform_id, cl_device_type, cl_uint,
                                     cl_device_id *, cl_uint *);
typedef cl_int (*clGetDeviceInfo_ptr)(cl_device_id, cl_device_info, size_t, void *, size_t *);

typedef cl_context (*clCreateContext_ptr)(const cl_context_properties *, cl_uint,
                                          const cl_device_id *,
                                          void (*)(const char *, const void *, size_t, void *),
                                          void *, cl_int *);
typedef cl_int (*clReleaseContext_ptr)(cl_context);

typedef cl_command_queue (*clCreateCommandQueue_ptr)(cl_context, cl_device_id,
                                                     cl_command_queue_properties, cl_int *);
typedef cl_int (*clReleaseCommandQueue_ptr)(cl_command_queue);

typedef cl_mem (*clCreateBuffer_ptr)(cl_context, cl_mem_flags, size_t, void *, cl_int *);
typedef cl_int (*clReleaseMemObject_ptr)(cl_mem);

typedef cl_program (*clCreateProgramWithSource_ptr)(cl_context, cl_uint,
                                                    const char **, const size_t *, cl_int *);
typedef cl_int (*clReleaseProgram_ptr)(cl_program);
typedef cl_int (*clBuildProgram_ptr)(cl_program, cl_uint, const cl_device_id *, const char *,
                                     void (*)(cl_program, void *), void *);

typedef cl_kernel (*clCreateKernel_ptr)(cl_program, const char *, cl_int *);
typedef cl_int (*clSetKernelArg_ptr)(cl_kernel, cl_uint, size_t, const void *);

typedef cl_int (*clEnqueueNDRangeKernel_ptr)(cl_command_queue, cl_kernel, cl_uint,
                                             const size_t *, const size_t *, const size_t *,
                                             cl_uint, const cl_event *, cl_event *);
typedef cl_int (*clGetProgramBuildInfo_ptr)(cl_program, cl_device_id, cl_program_build_info,
                                            size_t, void *, size_t *);
typedef cl_int (*clEnqueueReadBuffer_ptr)(cl_command_queue, cl_mem, cl_bool, size_t, size_t,
                                          void *, cl_uint, const cl_event *, cl_event *);
typedef cl_int (*clEnqueueWriteBuffer_ptr)(cl_command_queue, cl_mem, cl_bool, size_t, size_t,
                                           const void *, cl_uint, const cl_event *, cl_event *);



typedef struct _opencl_functions
{
	// platform API
	clGetPlatformIDs_ptr clGetPlatformIDs;
	clGetPlatformInfo_ptr clGetPlatformInfo;

	// device API
	clGetDeviceIDs_ptr clGetDeviceIDs;
	clGetDeviceInfo_ptr clGetDeviceInfo;

	// context API
	clCreateContext_ptr clCreateContext;
	clReleaseContext_ptr clReleaseContext;

	// command queue API
	clCreateCommandQueue_ptr clCreateCommandQueue;
	clReleaseCommandQueue_ptr clReleaseCommandQueue;

	// memory object API
	clCreateBuffer_ptr clCreateBuffer;
	clReleaseMemObject_ptr clReleaseMemObject;

	clEnqueueReadBuffer_ptr clEnqueueReadBuffer;
	clEnqueueWriteBuffer_ptr clEnqueueWriteBuffer;

	// program API
	clCreateProgramWithSource_ptr clCreateProgramWithSource;
	clReleaseProgram_ptr clReleaseProgram;
	clBuildProgram_ptr clBuildProgram;
	clGetProgramBuildInfo_ptr clGetProgramBuildInfo;

	// kernel API
	clCreateKernel_ptr clCreateKernel;
	clSetKernelArg_ptr clSetKernelArg;

	// flush & finish API

	// enqueue commands
	clEnqueueNDRangeKernel_ptr clEnqueueNDRangeKernel;

} OpenCL_func;

typedef struct _opencl_device
{
	cl_platform_id    platform_id;
	cl_device_id      device_id;

	cl_context context;
	cl_program program;
	cl_command_queue queue;

	cl_kernel intOutsideArtery, intInsideArtery;
	cl_kernel intOutsideVein,   intInsideVein;

	cl_mem mem_vein_buffer;
	cl_mem mem_pressures;
	cl_mem mem_results;

	cl_device_type    device_type;
	cl_uint           compute_units;
	size_t            max_work_item_size[2];
	size_t            max_work_group_size;

	cl_ulong          max_mem_size, max_alloc_size;
	cl_uint           cacheline_size;
	char              name[512];

	QByteArray        hash; // MD5 sum of name + device_type + pos in list (int)
	bool              device_enabled;

	struct CL_Vessel *cl_vessel; // 1024 vessels workspace
	struct CL_Result *integration_workspace; // 1024 results workspace

	_opencl_device() : hash(), device_enabled(true) {}
} OpenCL_device;

typedef std::list<OpenCL_device> OpenCLDeviceList;


struct CL_Vessel {
	cl_float R;
	cl_float pressure;
	cl_float GP;
	cl_float tone;
	cl_float flow;
	cl_float Ppl;

	cl_float a;
	cl_float b;
	cl_float c;

	cl_float peri_a;
	cl_float peri_b;
	cl_float peri_c;

	cl_float Kz;

	cl_float D;
	cl_float len;
	cl_float pad[1]; // pad to 64 bytes
};

struct CL_Result {
	cl_float R;
	cl_float D;
	cl_float Dmin;
	cl_float Dmax;
	cl_float vol;
	cl_float viscosity_factor;

	cl_float pad[10]; // pad to 64 bytes
};

class opencl_exception : public std::runtime_error
{
public:
	explicit opencl_exception(const std::string &name) throw() : std::runtime_error(name) {}
	explicit opencl_exception(const QString &name) throw() : std::runtime_error(name.toStdString()) {}
};

class OpenCL
{
public:
	OpenCL();
	~OpenCL();

	bool isAvailable() const;
	int nDevices() const { return n_devices; }

	OpenCLDeviceList devices() const;
	OpenCL_func functions() const;

	// throws opencl_exception on error
	static void errorCheck(cl_int status) throw(opencl_exception);

protected:
	void resolveFunctions();
	void addPlatform(cl_platform_id id);
	void addDevice(cl_platform_id, cl_device_id);

private:
	QByteArray program_src;
	OpenCL_func opencl;
	OpenCLDeviceList opencl_devices;
	int n_devices;
};
