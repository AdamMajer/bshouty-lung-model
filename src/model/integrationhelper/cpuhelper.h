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

	virtual double integrateBshoutyModel();
	virtual double laminarFlow();
	void integrateWithDimentions(Vessel::Type t,
	                             int gen, int idx,
	                             std::vector<double> &calc_dim);

protected:
	double laminarFlowVessel(Vessel &vessel);

	double integrateArtery(int vessel_index);
	double integrateVein(int vessel_index);

	double integrateVessel(Vessel::Type type,
	                       int vessel_index,
	                       std::vector<double> *calc_dim);
};
