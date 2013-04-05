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

#include "compromisemodel.h"
#include "math.h"

CompromiseModel::CompromiseModel(Model::Transducer trans,
                                 Model::IntegralType integral_type)
        : Model(trans, integral_type)
{
	target_pap = 25;
	slew_disease_idx = -1;
	param_no = 0;
}

CompromiseModel::CompromiseModel(const CompromiseModel &other)
        : Model(other)
{
	target_pap = other.target_pap;
	param_no = other.param_no;
	slew_disease_idx = other.slew_disease_idx;
}

CompromiseModel& CompromiseModel::operator =(const Model &other)
{
	Model::operator =(other);
	try {
		const CompromiseModel &o = dynamic_cast<const CompromiseModel&>(other);

		target_pap = o.target_pap;
		slew_disease_idx = o.slew_disease_idx;
		param_no = o.param_no;
	}
	catch(...) {
	}

	return *this;
}

bool CompromiseModel::setData(DataType type, double val)
{
	switch (type) {
	case Model::PAP_value:
		target_pap = val;
		return true;
	default:
		return Model::setData(type, val);
	}

	throw "Not Possible";
}

CompromiseModel* CompromiseModel::clone() const
{
	return new CompromiseModel(*this);
}

int CompromiseModel::calc(int max_iter)
{
	if (slew_disease_idx == -1 ||
	    slew_disease_idx >= static_cast<int>(dis.size()))
		return -1; // disease not specified

	if (!validInputs())
		return 0;

	double param_value;
	Range r = dis.at(slew_disease_idx).paramRange(param_no);
	const double orig_range = r.max() - r.min();
	CompromiseModel baseline(*this);
	dis[slew_disease_idx].setParameter(param_no, r.min());
	Model::calc();

	com_prog = 0;

	double new_pap = Model::getResult(Model::PAP_value);
	if (fabs(new_pap - target_pap) < Model::getResult(Model::Tlrns_value)) {
		return 1;
	}

	if (new_pap > target_pap) {
		// entered PAP is below expected value even with fully patent circulation
		return -2;
	}

	int i=0;
	param_value = r.min() + (r.max()-r.min())*(target_pap-new_pap)/target_pap;
	while(i++ < max_iter && !isAbort()) {
		static_cast<Model&>(*this) = baseline;
		dis[slew_disease_idx].setParameter(param_no, param_value);
		Model::calc();

		new_pap = Model::getResult(Model::PAP_value);
		double diff = fabs(new_pap - target_pap)/target_pap;
		if (diff < Model::getResult(Model::Tlrns_value)) {
			break;
		}

		if (new_pap < target_pap) {
			r.setMin(param_value);
			param_value = (param_value + r.max())/2;
		}
		else {
			r.setMax(param_value);
			param_value = (param_value+r.min())/2;
		}

		// estimate progress
		com_prog = 10000*(orig_range-r.max()+r.min())/orig_range;
	}

	n_iterations = i; // override n_iterations set by Model::calc()
	return i;
}

void CompromiseModel::setCalculatedParameter(const Disease &d, int p)
{
	for (DiseaseList::const_iterator i=dis.begin(); i!=dis.end(); ++i) {
		if (i->id() == d.id()) {
			slew_disease_idx = i - dis.begin();
			param_no = p;
			return;
		}
	}
}
