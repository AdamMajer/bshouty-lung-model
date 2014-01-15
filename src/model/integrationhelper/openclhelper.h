/*
 *   Bshouty Lung Model - Pulmonary Circulation Simulation
 *    Copyright (c) 1989-2014 Zoheir Bshouty, MD, PhD, FRCPC
 *    Copyright (c) 2011-2014 Adam Majer
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

#include "abstracthelper.h"
#include "model/model.h"
#include "opencl.h"
#include <QAtomicInt>
#include <QMutex>

struct OpenCLIntegrationData;
struct WorkGroup;
class CpuIntegrationHelper;
class OpenCLIntegrationHelper : public AbstractIntegrationHelper
{
public:
	OpenCLIntegrationHelper(Model *model, Model::IntegralType type);
	virtual ~OpenCLIntegrationHelper();

	virtual double multiSegmentedVessels();
	virtual double singleSegmentVessels();

	virtual double capillaryResistances();

	virtual bool isAvailable() const { return is_available; }
	virtual int hasErrors() const { return error; }

protected:
	float integrateByDevice(OpenCL_device &dev, cl_kernel k);
	float processWorkGroup(const struct WorkGroup &wg,
	                       OpenCL_device &dev,
	                       struct CL_Vessel *cl_vessel_buf,
	                       struct CL_Result *ret_values_buf);
	static int assignVessels(struct CL_Vessel*, const Vessel*, int n, int section_size);
	static void updateResults(const struct CL_Result*, Vessel*, int n);

private:
	bool is_available;
	int error;

	const int n_devices;
	std::vector<OpenCL_device> d;

	// used by the integration function
	QAtomicInt vein_index, art_index;
	CpuIntegrationHelper *cpu_helper; // used for capillaries
};
