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

#ifndef ABSTRACT_INTEGRATION_HELPER_H
#define ABSTRACT_INTEGRATION_HELPER_H

#include "model/model.h"

class AbstractIntegrationHelper
{

public:
	AbstractIntegrationHelper(Model *model);
	virtual ~AbstractIntegrationHelper();

	/* Integrate resistances across all arteries and veins and returns
	 * the maximum deviation from the pre-integration resistance value.
	 */
	virtual double integrate() = 0;

	virtual bool isAvailable() const { return true; }
	virtual bool hasErrors() const { return false; }

protected:
	bool isOutsideLung(int vessel_idx) const { return model->isOutsideLung(vessel_idx); }
	Vessel* arteries() { return model->arteries; }
	Vessel* veins() { return model->veins; }
	int nElements() const { return model->nElements(); }
	int nOutsideElements() const { return model->nOutsideElements(); }
	double LAP() const { return model->LAP; }
	double Hct() const { return model->Hct; }

private:
	Model *model;
};

#endif // ABSTRACT_INTEGRATION_HELPER_H
