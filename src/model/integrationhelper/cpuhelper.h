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

#include "abstracthelper.h"
#include "model/model.h"

class CpuIntegrationHelper : public AbstractIntegrationHelper
{
public:
	CpuIntegrationHelper(Model *model, Model::IntegralType type);

	virtual double multiSegmentedVessels();
	virtual double singleSegmentVessels();
	void integrateWithDimentions(Vessel::Type t,
	                             int gen, int idx,
	                             std::vector<double> &calc_dim);

	virtual double capillaryResistances();

protected:
	double capillaryResistance(Capillary &cap);
	double capillaryH(const Capillary &cap, double pressure);
	double singleSegmentVessel(Vessel &v);
	double multiSegmentedFlowVessel(Vessel &v);
	double multiSegmentedFlowVessel(Vessel &v,
	                                std::vector<double> *calc_dim);

	double vesselIntegration(double(CpuIntegrationHelper::* func)(Vessel&));
	double vesselIntegrationThread(double(CpuIntegrationHelper::* func)(Vessel&));

	double capillaryThread();

	QAtomicInt artery_no, vein_no, cap_no;
};
