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

#include "model.h"



class CompromiseModel : public Model
{
public:
	// use either PVOD or PAH model
	// will take ownership of baseline_model instance
	CompromiseModel(Model::Transducer trans, Model::IntegralType integral_type);
	CompromiseModel(const CompromiseModel &);

	virtual CompromiseModel& operator=(const Model &other);

	virtual bool setData(DataType, double);

	virtual CompromiseModel* clone() const;
	virtual int calc(int max_iter);

	void setCalculatedParameter(const Disease &d, int param_no);
	int targetParameterNo() const { return param_no; }

	virtual int progress() const { return com_prog.fetchAndAddRelaxed(0); }

private:
	int slew_disease_idx;
	double target_pap;
	unsigned param_no;
	mutable QAtomicInt com_prog;
};
