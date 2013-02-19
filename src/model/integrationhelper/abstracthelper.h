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
	AbstractIntegrationHelper(Model *model, Model::IntegralType solver);
	virtual ~AbstractIntegrationHelper();

	/* Integrate resistances across all arteries and veins and returns
	 * the maximum deviation from the pre-integration resistance value.
	 */
	double integrate();
	virtual double segmentedVessels() = 0;
	virtual double rigidVessels() = 0;

	virtual double capillaryResistances() = 0;

	virtual bool isAvailable() const { return true; }
	virtual bool hasErrors() const { return false; }

protected:
	bool isOutsideLung(int vessel_idx) const { return model->isOutsideLung(vessel_idx); }
	Vessel* arteries() { return model->arteries; }
	Vessel* veins() { return model->veins; }
	Capillary* capillaries() { return model->caps; }
	int nArteries() const { return model->numArteries(); }
	int nVeins() const { return model->numVeins(); }
	int nCaps() const { return model->numCapillaries(); }
	int index(int gen, int idx) const { return model->startIndex(gen)+idx; }
	double Hct() const { return model->Hct; }
	double Tlrns() const { return model->Tlrns; }

	double arteryRatio(int idx) const {
		const int gen = model->gen_no(idx);
		return model->nElements(gen) / (double)model->nVessels(Vessel::Artery, gen);
	}
	double veinRatio(int idx) const {
		const int gen = model->gen_no(idx);
		return model->nElements(gen) / (double)model->nVessels(Vessel::Vein, gen);
	}

private:
	Model *model;
	Model::IntegralType solver_type;
};

#endif // ABSTRACT_INTEGRATION_HELPER_H
