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

#include "../common.h"
#include <QtConcurrentRun>
#include <QFuture>
#include <QCoreApplication>
#include <QFutureSynchronizer>
#include <QFile>
#include <QProgressDialog>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <cmath>
#include "dbsettings.h"
#include "integrationhelper/cpuhelper.h"
#include "integrationhelper/openclhelper.h"
#include "model.h"
#include <limits>

#include <QSettings>
#include <QTime>

/* No accuracy benefit above 128. Speed is not compromised at 128 (on 16 core machine!) */
const int nSums = 128; // number of divisions in the integral
const QLatin1String vessel_ini_relative("data/vessel.ini");
#define vessel_ini (qApp->applicationDirPath() + "/" + vessel_ini_relative)

#if defined(Q_OS_WIN32) && !defined(__GNUC__)
inline double cbrt(double x) {
	if (x > 0)
		return pow(x, 1.0/3.0);

	return -pow(-x, 1.0/3.0);
}
#endif

bool operator==(const struct Vessel &a, const struct Vessel &b)
{
	return memcmp(&a, &b, sizeof(struct Vessel)) == 0 ||
	    !(significantChange(a.gamma, b.gamma) ||
	      significantChange(a.phi, b.phi) ||
	      significantChange(a.c, b.c) ||
	      significantChange(a.R, b.R) ||
	      significantChange(a.D, b.D) ||
	      significantChange(a.D_calc, b.D_calc) ||
	      significantChange(a.Dmin, b.Dmin) ||
	      significantChange(a.Dmax, b.Dmax) ||
	      significantChange(a.volume, b.volume) ||
	      significantChange(a.length, b.length) ||
	      significantChange(a.viscosity_factor, b.viscosity_factor) ||
	      significantChange(a.tone, b.tone) ||
	      significantChange(a.GP, b.GP) ||
	      significantChange(a.GPz, b.GPz) ||
	      significantChange(a.Ppl, b.Ppl) ||
	      significantChange(a.Ptp, b.Ptp) ||
	      significantChange(a.perivascular_press_a, b.perivascular_press_a) ||
	      significantChange(a.perivascular_press_b, b.perivascular_press_b) ||
	      significantChange(a.perivascular_press_c, b.perivascular_press_c) ||
	      significantChange(a.perivascular_press_d, b.perivascular_press_d) ||
	      significantChange(a.length_factor, b.length_factor) ||
	      significantChange(a.total_R, b.total_R) ||
	      significantChange(a.pressure_in, b.pressure_in) ||
	      significantChange(a.pressure_out, b.pressure_out) ||
	      significantChange(a.flow, b.flow));
}

bool operator==(const struct Capillary &a, const struct Capillary &b)
{
	return memcmp(&a, &b, sizeof(struct Capillary)) == 0 ||
	    !(significantChange(a.Alpha, b.Alpha) ||
	      significantChange(a.Ho, b.Ho) ||
	      significantChange(a.Krc, b.Krc));
}


// CONSTRUCTOR - always called - initializes everything
Model::Model(Transducer transducer_pos, IntegralType int_type)
{
	n_iterations = 0;
	vessel_value_override.resize(numArteries() + numVeins() + numCapillaries(), false);

	arteries = (Vessel*)allocateCachelineAligned(sizeof(Vessel)*numArteries());
	veins = (Vessel*)allocateCachelineAligned(sizeof(Vessel)*numVeins());
	caps = (Capillary*)allocateCachelineAligned(sizeof(Capillary)*numCapillaries());

	if (arteries == 0 || veins == 0 || caps == 0)
		throw std::bad_alloc();

	Krc_factor = calibrationValue(Krc);
	pat_gender = Male;

	/* set to zero, mostly to prevent valgrind complaining */
	memset(arteries, 0, numArteries()*sizeof(Vessel));
	memset(veins, 0, numVeins()*sizeof(Vessel));
	memset(caps, 0, numCapillaries()*sizeof(Capillary));

	// Initial conditions
	trans_pos = transducer_pos;
	Tlrns = calibrationValue(Tlrns_value);

	PatHt = calibrationValue(Pat_Ht_value);
	PatWt = calibrationValue(Pat_Wt_value);
	LungHt[0] = LungHt[1] = calibrationValue(Lung_Ht_value);

	Pal = calibrationValue(Pal_value);
	Ppl = calibrationValue(Ppl_value);

	CO = calibrationValue(CO_value);
	LAP = calibrationValue(LAP_value);

	Vm[0] = Vm[1] = calibrationValue(Vm_value);
	Vc[0] = Vc[1] = calibrationValue(Vc_value);
	Vd[0] = Vd[1] = calibrationValue(Vd_value);
	Vtlc[0] = Vtlc[1] = calibrationValue(Vtlc_value);

	Hct = calibrationValue(Hct_value);
	PA_EVL = calibrationValue(PA_EVL_value);
	PA_diam = calibrationValue(PA_Diam_value);
	PV_EVL = calibrationValue(PV_EVL_value);
	PV_diam = calibrationValue(PV_Diam_value);
	cv_diam_ratio = calibrationValue(CV_Diam_value);

	CI = CO/BSAz();
	integral_type = int_type;

	// to have a default PAP value of 15
	arteries[0].flow = CO;
	arteries[0].total_R = (15.0-LAP)/arteries[0].flow;

	modified_flag = true;
	model_reset = true;
	abort_calculation = 0;

	allocateIntegralType();

	// Initial conditions
	initVesselBaselineCharacteristics();
}

Model::Model(const Model &other)
{
	arteries = (Vessel*)allocateCachelineAligned(sizeof(Vessel)*numArteries());
	veins = (Vessel*)allocateCachelineAligned(sizeof(Vessel)*numVeins());
	caps = (Capillary*)allocateCachelineAligned(sizeof(Capillary)*numCapillaries());

	integral_type = other.integral_type;
	allocateIntegralType();
	operator =(other);
}

Model::~Model()
{
	freeAligned(arteries);
	freeAligned(veins);
	freeAligned(caps);

	delete integration_helper;
}

Model& Model::operator =(const Model &other)
{
	// Assigns all values from other other model to the current model
	pat_gender = other.pat_gender;
	Tlrns = other.Tlrns;
	Pal = other.Pal;
	Ppl = other.Ppl;
	CO = other.CO;
	CI = other.CI;
	LAP = other.LAP;

	for (int i=0; i<2; ++i) {
		LungHt[i] = other.LungHt[i];
		Vm[i] = other.Vm[i];
		Vc[i] = other.Vc[i];
		Vd[i] = other.Vd[i];
		Vtlc[i] = other.Vtlc[i];
	}

	PatWt = other.PatWt;
	PatHt = other.PatHt;
	trans_pos = other.trans_pos;

	BSA_ratio = other.BSA_ratio;

	Hct = other.Hct;
	PA_EVL = other.PA_EVL;
	PA_diam = other.PA_diam;
	PV_EVL = other.PV_EVL;
	PV_diam = other.PV_diam;
	cv_diam_ratio = other.cv_diam_ratio;

	modified_flag = other.modified_flag;
	model_reset = other.model_reset;
	abort_calculation = other.abort_calculation;

	if (integral_type != other.integral_type) {
		delete integration_helper;
		integral_type = other.integral_type;
		allocateIntegralType();
	}

	dis = other.dis;

	Krc_factor = other.Krc_factor;
	n_iterations = other.n_iterations;

	memcpy(arteries, other.arteries, numArteries()*sizeof(Vessel));
	memcpy(veins, other.veins, numVeins()*sizeof(Vessel));
	memcpy(caps, other.caps, numCapillaries()*sizeof(Capillary));

	vessel_value_override = other.vessel_value_override;

	prog = other.prog;

	return *this;
}

void Model::allocateIntegralType()
{
	bool opencl_helper = cl->isAvailable() &&
	                DbSettings::value(settings_opencl_enabled, true).toBool();

	if (opencl_helper)
		integration_helper = new OpenCLIntegrationHelper(this, integral_type);
	else
		integration_helper = new CpuIntegrationHelper(
		                             this,
		                             integral_type);

	if (integration_helper == 0)
		throw std::bad_alloc();
}

Model* Model::clone() const
{
	return new Model(*this);
}

void Model::setIntegralType(IntegralType t) {
	integral_type = t;
}

int Model::nVessels(Vessel::Type vessel_type,
                    unsigned gen_no) const
{
	/* As measured in Huang's paper - Morphometry of
	 * Pulmonary Vasculature, Table 5
	 */

	static int artery_number[16], vein_number[16];

	if (artery_number[0] == 0.0) {
		/* Initialize values */
		QSettings s(vessel_ini, QSettings::IniFormat);
		for (int i=1; i<=16; ++i) {
			artery_number[i-1] = s.value("artery_number/gen_"+QString::number(i)).toInt();
			vein_number[i-1] = s.value("vein_number/gen_"+QString::number(i)).toInt();
		}
	}


	switch (vessel_type) {
	case Vessel::Artery:
		// includes corner vessels, same as last vessels
		if (gen_no == 17)
			gen_no = 16;
		if (gen_no > 16)
			break;
		return artery_number[gen_no-1];
	case Vessel::Vein:
		if (gen_no > 16)
			break;
		return vein_number[gen_no-1];
	}

	return 0;
}

double Model::measuredDiameterRatio(Vessel::Type vessel_type,
                                    unsigned gen_no) const
{
	static double artery_ratios[16], vein_ratios[16];

	if (artery_ratios[0] == 0.0) {
		/* Initialize values */
		QSettings s(vessel_ini, QSettings::IniFormat);
		for (int i=1; i<=16; ++i) {
			artery_ratios[i-1] = s.value("artery_diameter_ratios/gen_"+QString::number(i)).toDouble();
			vein_ratios[i-1] =  s.value("vein_diameter_ratios/gen_"+QString::number(i)).toDouble();
		}
	}

	switch (vessel_type) {
	case Vessel::Artery:
		if (gen_no == 17)
			return cv_diam_ratio;
		if (gen_no > 16)
			break;
		return artery_ratios[gen_no-1];
	case Vessel::Vein:
		if (gen_no > 16)
			break;
		return vein_ratios[gen_no-1];
	}

	return 1;
}

double Model::measuredLengthRatio(Vessel::Type vessel_type,
                                  unsigned gen_no)
{
	/* Length ratios assume 2.5 cm main PA, 5cm left PA
	 * same for the veins
	 */
	static double artery_ratios[16], vein_ratios[16];

	if (artery_ratios[0] == 0.0) {
		/* Initialize values */
		QSettings s(vessel_ini, QSettings::IniFormat);
		for (int i=1; i<=16; ++i) {
			artery_ratios[i-1]=s.value("artery_length_ratios/gen_"+QString::number(i)).toDouble();
			vein_ratios[i-1]=s.value("vein_length_ratios/gen_"+QString::number(i)).toDouble();
		}
	}


	switch (vessel_type) {
	case Vessel::Artery:
		if (gen_no == 17)
			gen_no = 16; // corner vessels, same as last gen vessels
		if (gen_no > 16)
			break;
		return artery_ratios[gen_no-1];
	case Vessel::Vein:
		if (gen_no > 16)
			break;
		return vein_ratios[gen_no-1];
	}

	return 1;
}

double Model::BSAz() const
{
	return BSA(calibrationValue(Model::Pat_Ht_value),
	           calibrationValue(Model::Pat_Wt_value));
}

double Model::BSA(double pat_ht, double pat_wt)
{
	 return sqrt(pat_ht * pat_wt) / 60.0;
}

double Model::idealWeight(Gender gender, double pat_ht)
{
	return 22.0 * sqr(pat_ht/100.0);

	switch (gender) {
	case Model::Male:
		if (pat_ht > 123.558)
			return 50.0 + 2.3/2.54 * (pat_ht - 60*2.54);
		break;
	case Model::Female:
		if (pat_ht > 134.479)
			return 45.5 + 2.3/2.54 * (pat_ht - 60*2.54);
		break;
	}

	// pediatric ideal weight formula
	return 2.39*exp(0.01863*pat_ht);
}

const Vessel& Model::artery( int gen, int index ) const
{
	if( gen <= 0 || index < 0 || gen > 17 || index >= nElements( gen ))
		throw "Out of bounds";

	return arteries[ index + startIndex( gen )];
}

const Vessel& Model::vein( int gen, int index ) const
{
	if( gen <= 0 || index < 0 || gen > 16 || index >= nElements( gen ))
		throw "Out of bounds";

	return veins[ index + startIndex( gen )];
}

const Capillary& Model::capillary( int index ) const
{
	if( index < 0 || index >= nElements( 16 ))
		throw "Out of bounds";

	return caps[ index ];
}

void Model::setArtery(int gen, int index, const Vessel & v, bool override)
{
	if( gen <= 0 || index < 0 || gen > 17 || index >= nElements( gen ))
		throw "Out of bounds";

	const int idx = index + startIndex(gen);

	arteries[idx] = v;
	vessel_value_override[idx] = vessel_value_override[idx] || override;
	modified_flag = true;
}

void Model::setVein(int gen, int index, const Vessel & v, bool override)
{
	if( gen <= 0 || index < 0 || gen > 16 || index >= nElements( gen ))
		throw "Out of bounds";

	const int idx = index + startIndex(gen);
	const int o_idx = numArteries() + idx;

	veins[idx] = v;
	vessel_value_override[o_idx] = vessel_value_override[o_idx] || override;
	modified_flag = true;
}

void Model::setCapillary(int index, const Capillary & c, bool override)
{
	if( index < 0 || index >= nElements( 16 ))
		throw "Out of bounds";

	const int c_idx = index + numArteries() + numVeins();
	caps[ index ] = c;

	caps[index].F = 1.0 + 25*caps[index].Alpha;
	caps[index].F3 = caps[index].F*caps[index].F*caps[index].F;
	caps[index].F4 = caps[index].F*caps[index].F*caps[index].F*caps[index].F;

	vessel_value_override[c_idx] = vessel_value_override[c_idx] || override;
	modified_flag = true;
}

double Model::getResult(DataType type) const
{
	switch( type ){
	case Lung_Ht_L_value:
	case Lung_Ht_value:
		return LungHt[0];
	case Lung_Ht_R_value:
		return LungHt[1];
	case CO_value:
		return CO;
	case LAP_value:
		return LAP;
	case Pal_value:
		return Pal;
	case Ppl_value:
		return Ppl;
	case Ptp_value:
		return Pal - Ppl;
	case PAP_value:
		if (!validInputs())
			return std::numeric_limits<double>::quiet_NaN();

		return LAP + arteries[0].flow * arteries[0].total_R;
	case Rus_value:
		return arteries[0].partial_R;
	case Rds_value:
		return veins[0].partial_R;
	case Rm_value:
		return veins[0].total_R - arteries[0].partial_R - veins[0].partial_R;
	case Rt_value:
		return (getResult(PAP_value) - LAP) / CO;
	case Tlrns_value:
		return Tlrns;
	case Gender_value:
		return pat_gender;
	case Pat_Ht_value:
		return PatHt;
	case Pat_Wt_value:
		return PatWt;
	case TotalR_value:
		return arteries[0].total_R;
	case DiseaseParam:
		break;

	case Vm_L_value:
	case Vm_value:
		return Vm[0] * 100.0;
	case Vc_L_value:
	case Vc_value:
		return Vc[0];
	case Vd_L_value:
	case Vd_value:
		return Vd[0];
	case Vtlc_L_value:
	case Vtlc_value:
		return Vtlc[0] * 100.0;
	case Vm_R_value:
		return Vm[1] * 100.0;
	case Vc_R_value:
		return Vc[1];
	case Vd_R_value:
		return Vd[1];
	case Vtlc_R_value:
		return Vtlc[1] * 100.0;

	case Hct_value:
		return Hct * 100.0;
	case PA_EVL_value:
		return PA_EVL;
	case PA_Diam_value:
		return PA_diam;
	case PV_EVL_value:
		return PV_EVL;
	case PV_Diam_value:
		return PV_diam;
	case CV_Diam_value:
		return cv_diam_ratio * PA_diam;

	case Krc:
		return Krc_factor;
	}

	// Handle special case of DiseaseParam
	if ((type & DiseaseParam) == DiseaseParam) {
		const int dis_no = diseaseNo(type);
		const int param_no = diseaseParamNo(type);

		if (dis_no < static_cast<int>(dis.size()))
			return dis.at(dis_no).parameterValue(param_no);
	}

	throw "Should never get here";
}

bool Model::setData(DataType type, double val)
{
	switch( type ){
	case Lung_Ht_value:
		if (significantChange(LungHt[0], val) ||
		    significantChange(LungHt[1], val)) {
			LungHt[0] = LungHt[1] = val;
			modified_flag = true;
			calculateBaselineCharacteristics();
			return true;
		}
		break;
	case Lung_Ht_L_value:
		if (significantChange(LungHt[0], val)) {
			LungHt[0] = val;
			modified_flag = true;
			calculateBaselineCharacteristics();
			return true;
		}
		break;
	case Lung_Ht_R_value:
		if (significantChange(LungHt[1], val)) {
			LungHt[1] = val;
			modified_flag = true;
			calculateBaselineCharacteristics();
			return true;
		}
		break;
	case CO_value:
		if (significantChange(CO, val)) {
			CO = val;
			CI = CO / BSA(PatHt, PatWt);
			modified_flag = true;
			return true;
		}
		break;
	case LAP_value:
		if (significantChange(LAP, val)) {
			LAP = val;
			modified_flag = true;
			return true;
		}
		break;
	case Pal_value:
		if (significantChange(Pal, val)) {
			Pal = val;
			modified_flag = true;
			calculateBaselineCharacteristics();
			return true;
		}
		break;
	case Ppl_value:
		if (significantChange(Ppl, val)) {
			Ppl = val;
			modified_flag = true;
			calculateBaselineCharacteristics();
			return true;
		}
		break;
	case Tlrns_value:
		if (significantChange(Tlrns, val)) {
			Tlrns = val;
			modified_flag = true;
			return true;
		}
		break;
	case Vm_value:
		val /= 100.0;
		if (significantChange(Vm[0], val) ||
		    significantChange(Vm[1], val)) {
			Vm[0] = Vm[1] = val;
			calculateBaselineCharacteristics();
			modified_flag = true;
			return true;
		}
		break;
	case Vc_value:
		if (significantChange(Vc[0], val) ||
		    significantChange(Vc[1], val)) {
			Vc[0] = Vc[1] = val;
			calculateBaselineCharacteristics();
			modified_flag = true;
			return true;
		}
		break;
	case Vd_value:
		if (significantChange(Vd[0], val) ||
		    significantChange(Vd[1], val)) {
			Vd[0] = Vd[1] = val;
			calculateBaselineCharacteristics();
			modified_flag = true;
			return true;
		}
		break;
	case Vtlc_value:
		val /= 100.0;
		if (significantChange(Vtlc[0], val) ||
		    significantChange(Vtlc[1], val)) {
			Vtlc[0] = Vtlc[1] = val;
			calculateBaselineCharacteristics();
			modified_flag = true;
			return true;
		}
		break;
	case Vm_L_value:
		val /= 100.0;
		if (significantChange(Vm[0], val)) {
			Vm[0] = val;
			calculateBaselineCharacteristics();
			modified_flag = true;
			return true;
		}
		break;
	case Vc_L_value:
		if (significantChange(Vc[0], val)) {
			Vc[0] = val;
			calculateBaselineCharacteristics();
			modified_flag = true;
			return true;
		}
		break;
	case Vd_L_value:
		if (significantChange(Vd[0], val)) {
			Vd[0] = val;
			calculateBaselineCharacteristics();
			modified_flag = true;
			return true;
		}
		break;
	case Vtlc_L_value:
		val /= 100.0;
		if (significantChange(Vtlc[0], val)) {
			Vtlc[0] = val;
			calculateBaselineCharacteristics();
			modified_flag = true;
			return true;
		}
		break;
	case Vm_R_value:
		val /= 100.0;
		if (significantChange(Vm[1], val)) {
			Vm[1] = val;
			calculateBaselineCharacteristics();
			modified_flag = true;
			return true;
		}
		break;
	case Vc_R_value:
		if (significantChange(Vc[1], val)) {
			Vc[1] = val;
			calculateBaselineCharacteristics();
			modified_flag = true;
			return true;
		}
		break;
	case Vd_R_value:
		if (significantChange(Vd[1], val)) {
			Vd[1] = val;
			calculateBaselineCharacteristics();
			modified_flag = true;
			return true;
		}
		break;
	case Vtlc_R_value:
		val /= 100.0;
		if (significantChange(Vtlc[1], val)) {
			Vtlc[1] = val;
			calculateBaselineCharacteristics();
			modified_flag = true;
			return true;
		}
		break;
	case Hct_value:
		val /= 100.0;
		if (significantChange(Hct, val)) {
			Hct = val;
			modified_flag = true;
			return true;
		}
		break;
	case PA_EVL_value:
		if (significantChange(PA_EVL, val)) {
			PA_EVL = val;
			initVesselBaselineResistances();
			modified_flag = true;
			return true;
		}
		break;
	case PA_Diam_value:
		if (significantChange(PA_diam, val)) {
			PA_diam = val;
			initVesselBaselineResistances();
			modified_flag = true;
			return true;
		}
		break;
	case PV_EVL_value:
		if (significantChange(PV_EVL, val)) {
			PV_EVL = val;
			initVesselBaselineResistances();
			modified_flag = true;
			return true;
		}
		break;
	case PV_Diam_value:
		if (significantChange(PV_diam, val)) {
			PV_diam = val;
			initVesselBaselineResistances();
			modified_flag = true;
			return true;
		}
		break;
	case CV_Diam_value:
		if (significantChange(cv_diam_ratio*PA_diam, val)) {
			cv_diam_ratio = val / PA_diam;
			initVesselBaselineResistances();
			modified_flag = true;
			return true;
		}
		break;
	case Ptp_value:
	case PAP_value:
	case Rus_value:
	case Rds_value:
	case Rm_value:
	case Rt_value:
	case TotalR_value:
		throw "Cannot set calculated values!";
	/* For Height and Weight, we have to recalculate the entire lung baseline */
	case Pat_Ht_value:
		if (significantChange(PatHt, val)) {
			/* MPA axial diameter fit from Table 2
			 * (Knobel et al, "Geometry and dimensions of the pulmonary
			 *  artery bifurcation in children and adolescents: assessment
			 *  in vivo by contrast-enhanced MR-angiography)
			 */

			PatWt = idealWeight(pat_gender, val);
			PatHt = val;

			double idealBSA = BSA(calibrationValue(Model::Pat_Ht_value),
			                      calibrationValue(Model::Pat_Wt_value));
			double baseline_ratio = 4.85 + 13.43*sqrt(idealBSA);
			double new_ratio = 4.85 + 13.43*sqrt(BSA(PatHt, PatWt));

			PA_diam = calibrationValue(Model::PA_Diam_value)*new_ratio/baseline_ratio;
			PV_diam = calibrationValue(Model::PV_Diam_value)*new_ratio/baseline_ratio;

			initVesselBaselineResistances();
			modified_flag = true;
			return true;
		}
		break;
	case Pat_Wt_value:
		if (significantChange(PatWt, val)) {
			PatWt = val;

			double idealBSA = BSA(calibrationValue(Model::Pat_Ht_value),
			                      calibrationValue(Model::Pat_Wt_value));
			double baseline_ratio = 4.85 + 13.43*sqrt(idealBSA);
			double new_ratio = 4.85 + 13.43*sqrt(BSA(PatHt, PatWt));

			PA_diam = calibrationValue(Model::PA_Diam_value)*new_ratio/baseline_ratio;
			PV_diam = calibrationValue(Model::PV_Diam_value)*new_ratio/baseline_ratio;

			initVesselBaselineResistances();
			modified_flag = true;
			return true;
		}
		break;
	case DiseaseParam:
	case Gender_value:
		break;

	case Krc:
		setKrFactors(val);
		break;
	}

	// Handle special case of DiseaseParam
	if ((type & DiseaseParam) == DiseaseParam) {
		const int dis_no = diseaseNo(type);
		const int param_no = diseaseParamNo(type);

		if (dis_no < static_cast<int>(dis.size())) {
			dis[dis_no].setParameter(param_no, val);
			return true;
		}
		return false;
	}

	return false;
}

Model::Transducer Model::transducerPos() const
{
	return trans_pos;
}

void Model::setTransducerPos(Model::Transducer trans)
{
	if (trans_pos == trans)
		return;

	trans_pos = trans;
	modified_flag = true;
	calculateBaselineCharacteristics();
}

void Model::setGender(Gender g)
{
	 pat_gender = g;
	 setData(Pat_Wt_value, idealWeight(g, PatHt));
}

int Model::calc( int max_iter )
{
	abort_calculation = 0;
	prog = 0;
	n_iterations = 0;

	if (!validInputs())
		return 0;

	if (model_reset) {
		getParameters();
		for (DiseaseList::iterator i=dis.begin(); i!=dis.end(); ++i)
			i->processModel(*this);

		for (int i=0; i<numArteries(); ++i)
			arteries[i].pressure_0 = calculatePressure0(arteries[i]);
		for (int i=0; i<numVeins(); ++i)
			veins[i].pressure_0 = calculatePressure0(veins[i]);

		model_reset = false;
	}

	/* It is possible that the last capillary that is opened results in all
	 * capilaries to be closed. To remedy this situation, we allow for the
	 * final opened capillary to be re-closed once more
	 */
	QThreadPool *thread_pool = QThreadPool::globalInstance();
	int ideal_thread_count = thread_pool->maxThreadCount();
	if (ideal_thread_count<=1) {
		thread_pool->reserveThread();
		thread_pool->reserveThread();
		thread_pool->reserveThread();
		thread_pool->reserveThread();
		ideal_thread_count=4;
	}

	do {
		qDebug() << "---- Iteration: " << n_iterations << " -> " << n_iterations;
		qDebug() << "PAPm: " << getResult(Model::PAP_value);

		totalResistance(0, ideal_thread_count);
		vascPress(ideal_thread_count);

		int iter_prog = 10000*n_iterations/max_iter;
		if (prog < iter_prog)
			prog = iter_prog;

		n_iterations++;
	} while (!deltaR(ideal_thread_count) &&
	         (n_iterations < max_iter) &&
	         abort_calculation==0);

	partialR(Vessel::Artery, 0);
	partialR(Vessel::Vein, 0);

	modified_flag = true;
	return n_iterations;
}

int Model::calculationErrors() const
{
	return integration_helper->hasErrors();
}

bool Model::load(QSqlDatabase &db, int offset, QProgressDialog *progress)
{
	return loadDb(db, offset, progress);
}

bool Model::save(QSqlDatabase &db, int offset, QProgressDialog *progress)
{
	return initDb(db) &&
	       db.transaction() && saveDb(db, offset, progress) && db.commit();
}

bool Model::isModified() const
{
	return modified_flag;
}

void Model::setAbort()
{
	abort_calculation = 1;
}

int Model::addDisease(const Disease &d)
{
	dis.push_back(d);
	return dis.size()-1;
}

void Model::clearDiseases()
{
	dis.clear();
}

const Disease& Model::disease(DataType hybrid_type) const
{
	int n = diseaseNo(hybrid_type);
	return dis.at(n);
}

int Model::diseaseNo(DataType hybrid_type)
{
	const unsigned int i = hybrid_type;
	return (i >> 16) & 0xFF;
}

int Model::diseaseParamNo(DataType hybrid_type)
{
	const unsigned int i = hybrid_type;
	return i >> 24;
}

Model::DataType Model::diseaseHybridType(const Disease &disease, int param_no) const
{
	int index = 0;
	foreach (const Disease &d, dis) {
		if (d.id() == disease.id())
			return diseaseHybridType(index, param_no);

		index++;
	}

	return DiseaseParam;
}

Model::DataType Model::diseaseHybridType(int disease_no, int param_no)
{
	return (Model::DataType)((param_no<<24) | (disease_no<<16) | DiseaseParam);
}

void Model::setKrFactors(double Krc)
{
	Krc_factor = Krc;
	initVesselBaselineResistances();
}

double Model::getKrc()
{
	if (Krc_factor == 0.0)
		Krc_factor = calibrationValue(Krc);

	return Krc_factor;
}

bool Model::validInputs() const
{
	return true;
}

QString Model::calibrationPath(DataType type)
{
	return "/settings/calibration/" + QString::number(type);
}

double Model::calibrationValue(DataType type)
{
	QVariant ret = DbSettings::value(calibrationPath(type));

	if (!ret.isNull()) {
		bool fOK;
		double ret_value = ret.toDouble(&fOK);

		if (fOK) {
			/* NOTE: Stored calibration values are pre-scaled */
			switch (type) {
			case Model::Vm_value:
			case Model::Vm_L_value:
			case Model::Vm_R_value:
			case Model::Vtlc_value:
			case Model::Vtlc_L_value:
			case Model::Vtlc_R_value:
			case Model::Hct_value:
				return ret_value / 100.0;
			default:
				return ret_value;
			}
		}
	}


	switch (type) {
	case Model::Gender_value:
		return Model::Male;
	case Model::Tlrns_value:
		return 0.0001;
	case Model::Pat_Ht_value:
		return 175.0;
	case Model::Pat_Wt_value:
		return 67.375;
	case Model::Lung_Ht_value:
	case Model::Lung_Ht_L_value:
	case Model::Lung_Ht_R_value:
		return 20;
	case Model::Vm_value:
	case Model::Vm_L_value:
	case Model::Vm_R_value:
		return 0.1;
	case Model::Vc_value:
	case Model::Vc_L_value:
	case Model::Vc_R_value:
		return 8.0;
	case Model::Vd_value:
	case Model::Vd_L_value:
	case Model::Vd_R_value:
		return 8.0;
	case Model::Vtlc_value:
	case Model::Vtlc_L_value:
	case Model::Vtlc_R_value:
		return 1.0;
	case Model::CO_value:
		return 3.37801294 * BSA(175.0, 67.375);
	case Model::LAP_value:
		return 5.0;
	case Model::Pal_value:
		return 0.0;
	case Model::Ppl_value:
		return -5.0;

	case Model::Hct_value:
		return 0.45;
	case Model::PA_EVL_value:
	case Model::PV_EVL_value:
		return 5.0;
	case Model::PA_Diam_value:
		return 1.220876095;
	case Model::PV_Diam_value:
		return 1.531792941;

	case Model::Krc:
		return 1111708.196081574;
	case Model::CV_Diam_value:
		return 0.0002997219255;

	case Model::Ptp_value:
	case Model::PAP_value:
	case Model::Rus_value:
	case Model::Rds_value:
	case Model::Rm_value:
	case Model::Rt_value:
	case Model::TotalR_value:
	case Model::DiseaseParam:
		return std::numeric_limits<double>::quiet_NaN();
	}

	return 0.0;
}

double Model::calculatePressure0(const Vessel &v)
{
	// Newton's Method to first zero from the right.
	//
	// Since there is a possibility of more than 1 zero with certain parameters,
	// it is important to select Pv to the right in the linear region
	// of the function, hence Pv=75
	int i=0;
	double Pv = 75.0;
	double diff;
	do {
		double alpha = Pv - v.perivascular_press_c - v.Ppl;
		double ex = std::exp(-alpha/v.perivascular_press_d);

		double fx = alpha - v.perivascular_press_a + v.perivascular_press_c -
		            v.perivascular_press_b / (1.0 + ex);
		double fxp = 1.0 - (v.perivascular_press_b/v.perivascular_press_d)*ex/sqr(1.0+ex);

		double old = Pv;
		Pv = old - fx/fxp;
		diff = std::fabs(old - Pv);
	}while (i++<100 && diff > 1e-10);

	return Pv/cmH2O_per_mmHg + v.tone;
}

void Model::getParameters()
{
	/* This procedure gets the arterial and venous parameters
	 * that determine perivascular pressure
	 *
	 * Adjust length basedon Ptp
	 */
	int n = numArteries();
	for (int i=0; i<n; i++) {
		Vessel &art = arteries[i];

		art.perivascular_press_d = 15.6068;
		if (isOutsideLung(i)) {
			art.perivascular_press_a = 0.0;
			art.perivascular_press_b = 0.0;
			art.perivascular_press_c = 0.0;
		}
		else {
			art.perivascular_press_a = -5 - art.Ptp;
			art.perivascular_press_b = +16.0 + 1.8*art.Ptp;
			art.perivascular_press_c = -10.781 + 17.7509*
			                           std::exp(-0.0594107*art.Ptp);
		}

		art.length *= art.length_factor;
	}

	n = numVeins();
	for (int i=0; i<n; i++) {
		Vessel &vein = veins[i];

		vein.perivascular_press_d = 16.02287;
		if (isOutsideLung(i)) {
			vein.perivascular_press_a = 0.0;
			vein.perivascular_press_b = 0.0;
			vein.perivascular_press_c = 0.0;
		}
		else {
			vein.perivascular_press_a = -5 - vein.Ptp;
			vein.perivascular_press_b = +17.0 + 1.9*vein.Ptp;
			vein.perivascular_press_c = -12.2009 + 15.7122*
			                            std::exp(-0.0699041*vein.Ptp);
		}

		vein.length *= vein.length_factor;
	}
}

void Model::vascPress(int ideal_threads)
{
	// Calculate Flow (Q) and then Pressure (P) for each resistance
	veins[0].flow = CO;
	arteries[0].flow = CO;

	double PAP = LAP + arteries[0].flow * arteries[0].total_R;
	arteries[0].pressure_in = PAP;
	arteries[0].pressure_out = PAP - arteries[0].flow * arteries[0].R;
	veins[0].pressure_in = LAP + veins[0].flow * veins[0].R;
	veins[0].pressure_out = LAP;

	calculateChildrenFlowPress(0, ideal_threads);
}

double Model::totalResistance(int i, int ideal_threads)
{
	// get current generation and determine if this is final generation
	int gen = gen_no( i );
	if( gen == nGenerations()){
		int c_idx = i - startIndex(gen);
		int cv_idx = c_idx + startIndex(17);
		double R = arteries[i].R +
		           1.0 / (1.0/caps[c_idx].R + 1.0/arteries[cv_idx].R) +
		           veins[i].R;
		arteries[i].total_R = R;
		veins[i].total_R = R;

		return R;
	}

	// not the final generation, determine connection indexes
	int current_gen_start = startIndex( gen );
	int next_gen_start = startIndex( gen+1 );

	int connection_first = (i - current_gen_start) * 2 + next_gen_start;

	// Add the connecting resistances in parallel
	double R1, R2;
	if (ideal_threads > 1) {
		QFuture<double> r1 = QtConcurrent::run(this, &Model::totalResistance,
		                                       connection_first, ideal_threads/2);
		QFuture<double> r2 = QtConcurrent::run(this, &Model::totalResistance,
		                                       connection_first+1, ideal_threads/2);

		R1 = r1.result();
		R2 = r2.result();
	}
	else {
		R1 = totalResistance(connection_first, ideal_threads/2);
		R2 = totalResistance(connection_first+1, ideal_threads/2);
	}

	double R_tot;

	if (isinf(R1) && isinf(R2))
		R_tot = std::numeric_limits<double>::infinity();
	else if(isinf(R1))
		R_tot = arteries[i].R + R2 + veins[i].R;
	else if(isinf(R2))
		R_tot = arteries[i].R + R1 + veins[i].R;
	else
		R_tot = arteries[i].R + 1/(1/R1 + 1/R2) + veins[i].R;

	arteries[i].total_R = R_tot;
	veins[i].total_R = R_tot;

	return R_tot;
}

double Model::partialR(Vessel::Type type, int i)
{
	Vessel *v = NULL;

	switch (type) {
	case Vessel::Artery:
		v = arteries;
		break;
	case Vessel::Vein:
		v = veins;
		break;
	}
	int gen = gen_no(i);
	if (gen == nGenerations()) {
		if (v[i].flow == 0.0)
			v[i].partial_R = std::numeric_limits<double>::infinity();
		else
			v[i].partial_R = v[i].R;

		return v[i].partial_R;
	}

	// not the final generation, determine connection indexes
	int current_gen_start = startIndex( gen );
	int next_gen_start = startIndex( gen+1 );

	int connection_first = (i - current_gen_start) * 2 + next_gen_start;

	// partial R calculation
	const double R1 = partialR(type, connection_first);
	const double R2 = partialR(type, connection_first+1);
	double total_R;

	if (isinf(R1) && isinf(R2))
		total_R = std::numeric_limits<double>::infinity();
	else if(isinf(R1))
		total_R = v[i].R + R2;
	else if(isinf(R2))
		total_R = v[i].R + R1;
	else
		total_R = v[i].R + 1/(1/R1 + 1/R2);

	v[i].partial_R = total_R;
	return total_R;
}

void Model::calculateChildrenFlowPress(int i , int ideal_threads)
{
	int gen = gen_no( i );
	int current_gen_start = startIndex( gen );

	// last generation, no children
	if( gen == nGenerations()) {
		const int c_idx = i - current_gen_start;
		const int cv_idx = c_idx + startIndex(gen+1);

		caps[c_idx].flow = arteries[i].flow*arteries[cv_idx].R/(arteries[cv_idx].R + caps[c_idx].R);
		arteries[cv_idx].flow = arteries[i].flow - caps[c_idx].flow;

		if (Q_UNLIKELY(isnan(caps[c_idx].flow) || isnan(arteries[cv_idx].flow))) {
			caps[c_idx].flow = 0.0;
			arteries[cv_idx].flow = 0.0;
		}

		arteries[cv_idx].pressure_out = veins[i].pressure_in;
		arteries[cv_idx].pressure_in = arteries[i].pressure_out;

		caps[c_idx].pressure_in = cmH2O_per_mmHg*arteries[i].pressure_out - Pal;
		caps[c_idx].pressure_out = cmH2O_per_mmHg*veins[i].pressure_in - Pal;
		return;
	}

	// determine connecting indexes
	int next_gen_start = startIndex( gen+1 );
	int connection_first = ( i - current_gen_start ) * 2 + next_gen_start;

	// Calculate flow and pressure in children vessels based on the calculated flow and their resistances
	for( int con=connection_first; con<=connection_first+1; con++ ){
		if (isinf(arteries[con].total_R)) {
			// all vessels inside have no flow, pressure is only defined
			// until block. Later, it is undefined.
			arteries[con].pressure_in = arteries[i].pressure_out - (arteries[con].GP - arteries[i].GP)/cmH2O_per_mmHg;
			veins[con].pressure_out = veins[i].pressure_in - (veins[con].GP - veins[i].GP)/cmH2O_per_mmHg;

			arteries[con].flow = 0.0;
			veins[con].flow = 0.0;

			arteries[con].pressure_out = arteries[con].pressure_in;
			veins[con].pressure_in = veins[con].pressure_out;

			arteries[con].pressure_out =
			                isinf(arteries[con].R) ?
			                        std::numeric_limits<double>::quiet_NaN() :
			                        arteries[con].pressure_in;

			veins[con].pressure_in =
			                isinf(veins[con].R) ?
			                        std::numeric_limits<double>::quiet_NaN() :
			                        veins[con].pressure_out;
		}
		else {
			arteries[con].pressure_in = arteries[i].pressure_out - (arteries[con].GP - arteries[i].GP)/cmH2O_per_mmHg;
			veins[con].pressure_out = veins[i].pressure_in - (veins[con].GP - veins[i].GP)/cmH2O_per_mmHg;

			double flow = (arteries[con].pressure_in - veins[con].pressure_out) /
			              arteries[con].total_R;;
			if (flow < 0.0 || isnan(flow))
				flow = 0.0;

			veins[con].flow = flow;
			arteries[con].flow = flow;

			arteries[con].pressure_out = arteries[con].pressure_in - arteries[con].flow * arteries[con].R;
			veins[con].pressure_in = veins[con].pressure_out + veins[con].flow * veins[con].R;
		}
	}

	// Now, calculate the same thing for child generations
	if (ideal_threads>1) {
		QFuture<void> f1 = QtConcurrent::run(
		                           this, &Model::calculateChildrenFlowPress,
		                           connection_first, ideal_threads/2);
		QFuture<void> f2 = QtConcurrent::run(
		                           this, &Model::calculateChildrenFlowPress,
		                           connection_first+1, ideal_threads/2);

		f1.waitForFinished();
		f2.waitForFinished();
	}
	else {
		calculateChildrenFlowPress(connection_first, 0);
		calculateChildrenFlowPress(connection_first+1, 0);
	}

	// Calculate 'backward' pressure for static flow vessels since a block
	for( int con=connection_first; con<=connection_first+1; con++ ){
		if (arteries[i].flow == 0.0 && isnan(arteries[i].pressure_out)) {
			arteries[i].pressure_out = arteries[con].pressure_in +
			                           (arteries[con].GP - arteries[i].GP)/cmH2O_per_mmHg;
			if (!isinf(arteries[i].R))
				arteries[i].pressure_in = arteries[i].pressure_out;
		}

		if (veins[i].flow == 0.0 && isnan(veins[i].pressure_in)) {
			veins[i].pressure_in = veins[con].pressure_out +
			                       (veins[con].GP - veins[i].GP)/cmH2O_per_mmHg;
			if (!isinf(veins[i].R))
				veins[i].pressure_out = veins[i].pressure_in;
		}
	}

	// Check if we need to recalculate forward static pressure towards a blockage
	// This is only encoutered in multiple blockages scenario
	for( int con=connection_first; con<=connection_first+1; con++ ){
		bool redo_branch = false;

		if (arteries[i].flow == 0.0 &&
		    !isnan(arteries[i].pressure_out) &&
		    isnan(arteries[con].pressure_in)) {

			arteries[con].pressure_in = arteries[i].pressure_out -
			                            (arteries[con].GP - arteries[i].GP)/cmH2O_per_mmHg;
			if (!isinf(arteries[con].R)) {
				arteries[con].pressure_out = arteries[con].pressure_in;
				redo_branch = true;
			}
		}

		if (veins[i].flow == 0.0 &&
		    !isnan(veins[i].pressure_in) &&
		    isnan(veins[con].pressure_out)) {

			veins[con].pressure_out = veins[i].pressure_in -
			                          (veins[con].GP - veins[i].GP)/cmH2O_per_mmHg;
			if (!isinf(veins[con].R)) {
				veins[con].pressure_in = veins[con].pressure_out;
				redo_branch = true;
			}
		}

		if (redo_branch)
			calculateChildrenFlowPress(con, ideal_threads);
	}
}

bool Model::deltaR(int ideal_threads)
{
	// integrate resistance of veins and arteries
	double max_vessel_deviation = integration_helper->integrate();
	double max_cap_deviation = integration_helper->capillaryResistances();
	int cap_iteration = 0;

	qDebug() << "   max vessel deltaR: " << max_vessel_deviation;
	qDebug() << "   max cap deltaR:    " << max_cap_deviation;
	while (max_cap_deviation/max_vessel_deviation > 20.0 && cap_iteration < 25) {
		// unstable capillaries, simply adjust flow and recalculate
		// capillaries until deviation is reduced.
		vascPress(ideal_threads);
		max_cap_deviation = integration_helper->capillaryResistances();
		cap_iteration++;

		qDebug() << "    +++++  max cap deltaR: " << max_cap_deviation;
	}

	qDebug() << "   max cap deltaR: " << max_cap_deviation;

	// do not end iterations when capillaries are still ununstable
	double max_deviation = std::max(cap_iteration>0 ? Tlrns*100.0 : 0.0,
	                                std::max(max_vessel_deviation, max_cap_deviation));
	int estimated_progression = 10000;
	for (double md=max_deviation; md>Tlrns && estimated_progression>0;) {
		estimated_progression /= 2;
		md /= 2.0;
	}
	if (prog < estimated_progression)
		prog = estimated_progression;

	return max_deviation < Tlrns;
}

void Model::initVesselBaselineCharacteristics()
{
	initVesselBaselineResistances();
	calculateBaselineCharacteristics();

	/* Initialize vessel parameters */
	const int num_arteries = numArteries();
	for (int i=0; i<num_arteries; ++i) {
		if (vessel_value_override[i])
			continue;

		//arteries[i].a = 0.2419 / 1.2045;
		arteries[i].gamma = 1.84;
		arteries[i].phi = 0.04; //0.0275 / 1.2045;
		arteries[i].c = 0;
		arteries[i].tone = 0;

		int gen = gen_no(i);
		int n_arteries = nElements(gen);
		if (gen > 16)
			n_arteries = nElements(16);
		arteries[i].vessel_ratio = static_cast<double>(n_arteries) /
		                        nVessels(Vessel::Artery, gen);
	}

	const int num_veins = numVeins();
	for (int i=0; i<num_veins; ++i) {
		if (vessel_value_override[num_arteries + i])
			continue;

		if (isOutsideLung(i)) {
			// correct for vessels outside the lung
			veins[i].gamma = 1.85;
			veins[i].phi = 0.0;
		}
		else {
			veins[i].gamma = 1.85;
			veins[i].phi = 0.035;
		}
		veins[i].tone = 0;

		veins[i].vessel_ratio = static_cast<double>(nElements(gen_no(i))) /
		                        nVessels(Vessel::Vein, gen_no(i));
	}
}

void Model::initVesselBaselineResistances()
{
	CO = CI * BSA(PatHt, PatWt);

	initVesselBaselineResistances(1);

	// Initialize capillaries
	const int nCapillaries = numCapillaries();
	BSA_ratio = BSAz() / BSA(PatHt, PatWt);
	const double cKrc = getKrc() * BSA_ratio;
	for (int i=0; i<nCapillaries; i++) {
		if (vessel_value_override[numArteries() + numVeins() + i])
			continue;

		caps[i].R = cKrc;
		caps[i].Krc = cKrc;
		caps[i].open_state = Capillary_Auto;
	}
}

void Model::initVesselBaselineResistances(int gen)
{
	// Get starting index and number of elements in the generation
	int start_index = startIndex( gen );
	int num_elements = nElements( gen );

	bool is_corner_vessel = (gen == 17);
	if (is_corner_vessel) {
		num_elements = nElements(16);
	}
	int n = num_elements + start_index;

	// Initialize elements in Arteries and Veins
	const int n_arteries = numArteries();
	const double art_ratio = num_elements/(double)nVessels(Vessel::Artery, gen);
	const double vein_ratio = num_elements/(double)nVessels(Vessel::Vein, gen);
	const double Mi = 3.2; // viscosity constatant
	const double Kr = 1.2501e8 * 64.0 * Mi / M_PI; // constant for R(D,L) calculation;
	const double Kra_factor = Kr*art_ratio;
	const double Krv_factor = Kr*vein_ratio;

	for( int i=start_index; i<n; i++ ){
		int vessel_no = i - start_index;

		/* calculate baseline vessel diameters from resistances
		 *
		 * Initially blood is assumed to have a normal Hct of 0.45 and
		 * a normal viscosity of 3.2 (the units are centiPoise).
		 *
		 * From the equations R=8*Pi*Mi*L/A^2
		 *
		 * One can calculate baseline vessel cross-sectional area using
		 * the following relationship:
		 *
		 * R(baseline)*8000=8*pi*3.2*L/A(baseline)^2
		 */

		if (!vessel_value_override[i]) {
			const double art_d = 1e4 * PA_diam * measuredDiameterRatio(Vessel::Artery, gen);
			arteries[i].length = 1e4 * PA_EVL * measuredLengthRatio(Vessel::Artery, gen);
			arteries[i].D = art_d;
			arteries[i].R = Kra_factor*arteries[i].length/sqr(sqr(art_d));
			arteries[i].viscosity_factor = 1.0;
			arteries[i].volume = 1e-9 * M_PI/4.0*art_d*art_d*arteries[i].length / art_ratio;
		}

		if (!is_corner_vessel && !vessel_value_override[n_arteries+i]) {
			const double vein_d = 1e4 * PV_diam * measuredDiameterRatio(Vessel::Vein, gen);
			veins[i].length = 1e4 * PV_EVL * measuredLengthRatio(Vessel::Vein, gen);
			veins[i].D = vein_d;
			veins[i].R = Krv_factor*veins[i].length/sqr(sqr(vein_d));
			veins[i].viscosity_factor = 1.0;
			veins[i].volume = 1e-9 * M_PI/4.0*vein_d*vein_d*veins[i].length / vein_ratio;
		}

		if (!is_corner_vessel) {
			// GP was calculated with GP=0 being top of lung
			// then corrected based on transducer position
			// GPz is a fraction of lung height
			int gp_gen = gen;
			int effective_ngen = 16-1;
			if (gen > 1) {
				gp_gen--;
				vessel_no %= nElements(gp_gen);
			}

			const double GPz = (vessel_no*exp((effective_ngen-gp_gen+1)*M_LN2)+
			                 exp((effective_ngen-gp_gen)*M_LN2)-1) /
			                (exp((effective_ngen)*M_LN2) - 2);

			if (!vessel_value_override[i])
				arteries[i].GPz = GPz;
			if (!vessel_value_override[n_arteries+i])
				veins[i].GPz = GPz;
		}
		else {
			arteries[i].GPz = arteries[i-nElements(16)].GPz;
		}
	}

	// Initialize more generations, if they exist, including corner vessels
	if( gen <= nGenerations())
		initVesselBaselineResistances(gen+1);
}

void Model::calculateBaselineCharacteristics()
{
	/* This function corrects for transducer position and
	 * calculates vessel Ppl and vessel Ptp based on transducer position
	 * as well as global Ppl and Pal
	 */

	const int num_arteries = numArteries();
	const int num_veins = numVeins();
	for (int i=0; i<num_arteries; ++i) {
		Vessel &art = arteries[i];
		int gen = gen_no(i);
		int start_idx = startIndex(gen);
		int lung_no = lungSide(gen, i-start_idx);

		switch (trans_pos) {
		case Top:
			art.GP = -art.GPz*LungHt[lung_no];
			break;
		case Middle:
			art.GP = LungHt[lung_no]/2 - art.GPz*LungHt[lung_no];
			break;
		case Bottom:
			art.GP = LungHt[lung_no]-art.GPz*LungHt[lung_no];
			break;
		}

		// for every 1 cmH2O GP, Ppl changes by 0.55
		art.Ppl = Ppl - 0.55*art.GP;
		art.Ptp = Pal - art.Ppl;

		if (art.Ptp < 0)
			art.Ptp = 0;

		if (isOutsideLung(i)) {
			art.length_factor = 1.0;
		}
		else {
			art.length_factor = lengthFactor(art, lung_no);
		}
	}

	for (int i=0; i<num_veins; ++i) {
		Vessel &vein = veins[i];
		int gen = gen_no(i);
		int start_idx = startIndex(gen);
		int lung_no = lungSide(gen, i-start_idx);

		switch (trans_pos) {
		case Top:
			vein.GP = -vein.GPz*LungHt[lung_no];
			break;
		case Middle:
			vein.GP = LungHt[lung_no]/2 - vein.GPz*LungHt[lung_no];
			break;
		case Bottom:
			vein.GP = LungHt[lung_no]-vein.GPz*LungHt[lung_no];
			break;
		}

		// for every 1 cmH2O GP, Ppl changes by 0.55
		vein.Ppl = Ppl - 0.55*vein.GP;
		vein.Ptp = Pal - vein.Ppl;

		if (vein.Ptp < 0)
			vein.Ptp = 0;

		if (isOutsideLung(i)) {
			vein.length_factor = 1.0;
		}
		else {
			vein.length_factor = lengthFactor(vein, lung_no);
		}
	}

	// Initialize capillaries
	const int num_capillaries = numCapillaries();
	const int start_offset = startIndex(16);

	for (int i=0; i<num_capillaries; i++) {
		Capillary cap = caps[i];
		cap.Ho = 2.5;
		cap.open_state = Capillary_Auto;

		const Vessel &connected_vessel = arteries[i+start_offset];
		if (vessel_value_override[num_arteries + num_veins + i])
			continue;

		double cap_ppl = Ppl - 0.55*connected_vessel.GP;
		double cap_ptp = Pal - cap_ppl;
		cap.Alpha = 0.073+0.2*exp(-0.141*cap_ptp);

		setCapillary(i, cap, false);
	}
}

double Model::lengthFactor(const Vessel &v, int lung_no) const
{
	double cd = exp(Vc[lung_no]/Vd[lung_no]);
	double a = (Vm[lung_no]*(1+cd)-Vtlc[lung_no])/cd;
	double b = Vtlc[lung_no]-a;

	double V = a + b/(1+exp((Vc[lung_no] - v.Ptp)/Vd[lung_no]));
	double V0 = a + b/(1+exp(Vc[lung_no]/Vd[lung_no]));
	return cbrt(V / V0);
}

bool Model::initDb(QSqlDatabase &db) const
{
	static const QList<QStringList> stms = QList<QStringList>()
	    << (QStringList()
	        << "CREATE TABLE model_values (key NOT NULL, offset INTEGER NOT NULL, "
	                   "value NUMERIC NOT NULL, PRIMARY KEY(key,offset))"
	        << "CREATE TABLE vessel_values (type INTEGER NOT NULL, "
	                   "vessel_idx INTEGER NOT NULL, "
	                   "key NOT NULL, offset INTEGER NOT NULL, "
	                   "value NUMERIC NOT NULL, "
	                   "PRIMARY KEY(type,vessel_idx,key,offset))"
	        << "CREATE TABLE diseases (disease_id INTEGER NOT NULL PRIMARY KEY, script TEXT)"
	        << "CREATE TABLE disease_param (disease_id INTEGER NOT NULL, "
	                   "param_no INTEGER NOT NULL, "
	                   "param_value NOT NULL)"
	        << "CREATE TABLE model_version (version INT NOT NULL PRIMARY KEY)"
	        << "INSERT INTO model_version VALUES (1)"
	        );

	QSqlQuery q(db);
	int ver = 0;
	if (q.exec("SELECT version FROM model_version") && q.next()) {
		ver = q.value(0).toInt();
	}

	db.transaction();
	for (QList<QStringList>::const_iterator i=stms.begin()+ver; i!=stms.end(); ++i) {
		foreach (const QString &stm, *i) {
			if (!q.exec(stm))
				return false;
		}
	}

	return db.commit();
}


#define SET_VALUE(a) values.insert(#a, a)
#define GET_VALUE(a) if(!values.contains(#a)) return false; a = values.value(#a)

bool Model::saveDb(QSqlDatabase &db, int offset, QProgressDialog *progress)
{
	/* Assumption: progress is to advance 1000 steps during execution of this function */
	QSqlQuery q(db);

	// save each individual value
	QMap<QString,double> values;
	bool ret = true;

	SET_VALUE(Tlrns);
	SET_VALUE(LungHt[0]);
	SET_VALUE(LungHt[1]);
	SET_VALUE(Pal);
	SET_VALUE(Ppl);
	SET_VALUE(CO);
	SET_VALUE(CI);
	SET_VALUE(LAP);
	SET_VALUE(PatWt);
	SET_VALUE(PatHt);
	SET_VALUE(Hct);
	SET_VALUE(PA_EVL);
	SET_VALUE(PV_EVL);
	SET_VALUE(cv_diam_ratio);

	SET_VALUE(Vm[0]);
	SET_VALUE(Vm[1]);
	SET_VALUE(Vc[0]);
	SET_VALUE(Vc[1]);
	SET_VALUE(Vd[0]);
	SET_VALUE(Vd[1]);
	SET_VALUE(Vtlc[0]);
	SET_VALUE(Vtlc[1]);

	SET_VALUE(Krc_factor);

	q.exec("DELETE FROM model_values WHERE offset=" + QString::number(offset));
	q.prepare("INSERT INTO model_values (key, offset, value) VALUES (?, ?, ?)");
	for (QMap<QString,double>::const_iterator i=values.begin(); i!=values.end(); ++i) {
		q.addBindValue(i.key());
		q.addBindValue(offset);
		q.addBindValue(i.value());
		ret = q.exec() && ret;
	}

	q.addBindValue("transducer");
	q.addBindValue(offset);
	q.addBindValue((int)trans_pos);
	q.exec();

	q.addBindValue("gender");
	q.addBindValue(offset);
	q.addBindValue((int)pat_gender);
	q.exec();

	q.addBindValue("integral_type");
	q.addBindValue(offset);
	q.addBindValue((int)integral_type);
	q.exec();

	if (progress)
		progress->setValue(progress->value()+1);

	double div_per_vessel = (nElements()*2+nElements(16)) * 0.999 / 998.0;
	double progress_value=0, current_value=0;

	// save each vessel value
	values.clear();

	q.exec("DELETE FROM vessel_values WHERE offset=" + QString::number(offset));
	q.prepare("INSERT INTO vessel_values (type, vessel_idx, key, offset, value) "
	          "VALUES (?, ?, ?, ?, ?)");
	for (int type=1; type<=2; ++type) {
		int n_elements = (type==1 ? numArteries() : numVeins());
		for (int n=0; n<n_elements; ++n) {
			const int override_offset = (type==1 ? n : numArteries()+n);
			const Vessel &v = (type==1 ? arteries[n] : veins[n]);

			if (!vessel_value_override[override_offset])
				continue;

			SET_VALUE(v.R);
			SET_VALUE(v.D);
			SET_VALUE(v.Dmin);
			SET_VALUE(v.Dmax);
			SET_VALUE(v.D_calc);
			SET_VALUE(v.volume);
			SET_VALUE(v.length);
			SET_VALUE(v.viscosity_factor);

			SET_VALUE(v.gamma);
			SET_VALUE(v.phi);
			SET_VALUE(v.c);
			SET_VALUE(v.tone);

			SET_VALUE(v.GP);
			SET_VALUE(v.GPz);
			SET_VALUE(v.Ppl);
			SET_VALUE(v.Ptp);

			SET_VALUE(v.perivascular_press_a);
			SET_VALUE(v.perivascular_press_b);
			SET_VALUE(v.perivascular_press_c);
			SET_VALUE(v.perivascular_press_d);

			SET_VALUE(v.length_factor);

			SET_VALUE(v.total_R);
			SET_VALUE(v.pressure_in);
			SET_VALUE(v.pressure_out);
			SET_VALUE(v.flow);

			SET_VALUE(v.vessel_ratio);

			for (QMap<QString,double>::const_iterator i=values.begin();
			     i!=values.end();
			     ++i) {

				q.addBindValue(type);
				q.addBindValue(n);
				q.addBindValue(i.key());
				q.addBindValue(offset);
				q.addBindValue(i.value());

				ret = q.exec() && ret;
			}

			progress_value += div_per_vessel;
			while (progress && progress_value - current_value > 1.0) {
				progress->setValue(progress->value()+1);
				current_value += 1.0;
			}
		}
	}

	int n_caps = numCapillaries();
	values.clear();

	for (int n=0; n<n_caps; ++n) {
		const int override_offset = numArteries() + numVeins() + n;
		const Capillary &cap = caps[n];

		if (!vessel_value_override[override_offset])
			continue;

		SET_VALUE(cap.R);
		SET_VALUE(cap.Ho);
		SET_VALUE(cap.Alpha);


		for (QMap<QString,double>::const_iterator i=values.begin();
		     i!=values.end();
		     ++i) {

			q.addBindValue(3);
			q.addBindValue(n);
			q.addBindValue(i.key());
			q.addBindValue(offset);
			q.addBindValue(i.value());

			ret = q.exec() && ret;
		}

		progress_value += div_per_vessel;
		while (progress && progress_value - current_value > 1.0) {
			progress->setValue(progress->value()+1);
			current_value += 1.0;
		}
	}

	// save diseases that were added to the model, but only for baseline data
	if (offset == 0) {
		q.exec("DELETE FROM diseases");
		q.prepare("INSERT INTO diseases (disease_id, script) VALUES (?,?)");
		int n=0;

		QSqlQuery disease_param(db);
		disease_param.exec("DELETE FROM disease_param");
		disease_param.prepare("INSERT INTO disease_param (disease_id, param_no, param_value) "
		                      "VALUES (?,?,?)");
		for (DiseaseList::const_iterator i=dis.begin(); i!=dis.end(); ++i, ++n) {
			q.addBindValue(n);
			q.addBindValue(i->script());
			ret = q.exec() && ret;

			const int n_params = i->paramCount();
			for (int param_no=0; param_no<n_params; ++param_no) {
				disease_param.addBindValue(n);
				disease_param.addBindValue(param_no);
				disease_param.addBindValue(i->parameterValue(param_no));
				ret = disease_param.exec() && ret;
			}
		}

		if (progress)
			progress->setValue(progress->value()+1);
	}

	modified_flag = !ret;
	return ret;
}

bool Model::loadDb(QSqlDatabase &db, int offset, QProgressDialog *progress)
{
	/* Assumption: progress is to advance 1000 steps during execution of this function */
	QSqlQuery q(db);

	// load values
	QMap<QString,double> values;

	SET_VALUE(Tlrns);
	SET_VALUE(LungHt[0]);
	SET_VALUE(LungHt[1]);
	SET_VALUE(Pal);
	SET_VALUE(Ppl);
	SET_VALUE(CO);
	SET_VALUE(CI);
	SET_VALUE(LAP);
	SET_VALUE(PatWt);
	SET_VALUE(PatHt);
	SET_VALUE(Hct);
	SET_VALUE(PA_EVL);
	SET_VALUE(PV_EVL);
	SET_VALUE(cv_diam_ratio);

	SET_VALUE(Vm[0]);
	SET_VALUE(Vm[1]);
	SET_VALUE(Vc[0]);
	SET_VALUE(Vc[1]);
	SET_VALUE(Vd[0]);
	SET_VALUE(Vd[1]);
	SET_VALUE(Vtlc[0]);
	SET_VALUE(Vtlc[1]);

	SET_VALUE(Krc_factor);

	BSA_ratio = BSAz() / BSA(PatWt, PatHt);

	q.prepare("SELECT value FROM model_values WHERE key = ? AND offset = ?");

	q.addBindValue("transducer");
	q.addBindValue(offset);
	if (!q.exec() || !q.next()) {
		qDebug() << q.lastError().text();
		return false;
	}
	trans_pos = (Transducer)q.value(0).toInt();

	q.addBindValue("gender");
	q.addBindValue(offset);
	if (!q.exec() || !q.next())
		pat_gender = Model::Male;
	else
		pat_gender = (Model::Gender)q.value(0).toInt();

	q.addBindValue("integral_type");
	q.addBindValue(offset);
	if (!q.exec() || !q.next())
		integral_type = SegmentedVesselFlow;
	else
		integral_type = (IntegralType)q.value(0).toInt();

	*this = Model(trans_pos, integral_type);

	// Load all values
	for (QMap<QString,double>::iterator i=values.begin(); i!=values.end(); ++i) {
		q.addBindValue(i.key());
		q.addBindValue(offset);
		if (!q.exec() || !q.next())
			return false;

		i.value() = q.value(0).toDouble();
	}

	GET_VALUE(Tlrns);
	GET_VALUE(LungHt[0]);
	GET_VALUE(LungHt[1]);
	GET_VALUE(Pal);
	GET_VALUE(Ppl);
	GET_VALUE(CO);
	GET_VALUE(CI);
	GET_VALUE(LAP);
	GET_VALUE(PatWt);
	GET_VALUE(PatHt);
	GET_VALUE(Hct);
	GET_VALUE(PA_EVL);
	GET_VALUE(PV_EVL);
	GET_VALUE(cv_diam_ratio);

	GET_VALUE(Vm[0]);
	GET_VALUE(Vm[1]);
	GET_VALUE(Vc[0]);
	GET_VALUE(Vc[1]);
	GET_VALUE(Vd[0]);
	GET_VALUE(Vd[1]);
	GET_VALUE(Vtlc[0]);
	GET_VALUE(Vtlc[1]);

	GET_VALUE(Krc_factor);

	// force recalculation of Kr factors
	setKrFactors(Krc_factor);

	// load values of each vessel
	values.clear();

	if (progress)
		progress->setValue(progress->value()+1);

	double div_per_vessel = 998.0 / (nElements()*2+nElements(16));
	double progress_value=0, current_value=0;

	q.prepare("SELECT value FROM vessel_values WHERE type=? AND vessel_idx=? AND key=? AND offset=?");

	// restore defaults before we load overrides from file
	std::fill(vessel_value_override.begin(), vessel_value_override.end(), false);
	initVesselBaselineCharacteristics();

	QSqlQuery saved_elements(db);
	for (int type=1; type<=2; ++type) {
		std::vector<int> vessel_idx;

		saved_elements.exec("SELECT vessel_idx FROM vessel_values WHERE vessel_idx IS NOT NULL AND type=" + QString::number(type));
		while (saved_elements.next())
			vessel_idx.push_back(saved_elements.value(0).toInt());

		foreach (int n, vessel_idx) {
			const int override_offset = (type==1 ? n : numArteries()+n);
			Vessel &v = (type==1 ? arteries[n] : veins[n]);

			vessel_value_override[override_offset] = true;

			SET_VALUE(v.R);
			SET_VALUE(v.D);
			SET_VALUE(v.Dmin);
			SET_VALUE(v.Dmax);
			SET_VALUE(v.D_calc);
			SET_VALUE(v.volume);
			SET_VALUE(v.length);
			SET_VALUE(v.viscosity_factor);

			SET_VALUE(v.gamma);
			SET_VALUE(v.phi);
			SET_VALUE(v.c);
			SET_VALUE(v.tone);

			SET_VALUE(v.GP);
			SET_VALUE(v.GPz);
			SET_VALUE(v.Ppl);
			SET_VALUE(v.Ptp);

			SET_VALUE(v.perivascular_press_a);
			SET_VALUE(v.perivascular_press_b);
			SET_VALUE(v.perivascular_press_c);
			SET_VALUE(v.perivascular_press_d);

			SET_VALUE(v.length_factor);

			SET_VALUE(v.total_R);
			SET_VALUE(v.pressure_in);
			SET_VALUE(v.pressure_out);
			SET_VALUE(v.flow);

			SET_VALUE(v.vessel_ratio);

			for (QMap<QString,double>::iterator i=values.begin();
			     i!=values.end();
			     ++i) {

				q.addBindValue(type);
				q.addBindValue(n);
				q.addBindValue(i.key());
				q.addBindValue(offset);

				if (!q.exec() || !q.next())
					return false;

				i.value() = q.value(0).toDouble();
			}

			GET_VALUE(v.R);
			GET_VALUE(v.D);
			GET_VALUE(v.Dmin);
			GET_VALUE(v.Dmax);
			GET_VALUE(v.D_calc);
			GET_VALUE(v.volume);
			GET_VALUE(v.length);
			GET_VALUE(v.viscosity_factor);

			GET_VALUE(v.gamma);
			GET_VALUE(v.phi);
			GET_VALUE(v.c);
			GET_VALUE(v.tone);

			GET_VALUE(v.GP);
			GET_VALUE(v.GPz);
			GET_VALUE(v.Ppl);
			GET_VALUE(v.Ptp);

			GET_VALUE(v.perivascular_press_a);
			GET_VALUE(v.perivascular_press_b);
			GET_VALUE(v.perivascular_press_c);
			GET_VALUE(v.perivascular_press_d);

			GET_VALUE(v.length_factor);

			GET_VALUE(v.total_R);
			GET_VALUE(v.pressure_in);
			GET_VALUE(v.pressure_out);
			GET_VALUE(v.flow);

			GET_VALUE(v.vessel_ratio);

			progress_value += div_per_vessel;
			while (progress && progress_value - current_value > 1.0) {
				progress->setValue(progress->value()+1);
				current_value += 1.0;
				qApp->processEvents();
			}
		}
	}

	values.clear();

	std::vector<int> vessel_idx;
	saved_elements.exec("SELECT vessel_idx FROM vessel_values WHERE vessel_idx IS NOT NULL AND type=3");
	while (saved_elements.next())
		vessel_idx.push_back(saved_elements.value(0).toInt());

	foreach (int n, vessel_idx) {
		Capillary cap;

		SET_VALUE(cap.R);
		SET_VALUE(cap.Ho);
		SET_VALUE(cap.Alpha);


		for (QMap<QString,double>::iterator i=values.begin();
		     i!=values.end();
		     ++i) {

			q.addBindValue(3);
			q.addBindValue(n);
			q.addBindValue(i.key());
			q.addBindValue(offset);

			if (!q.exec() || !q.next())
				return false;

			i.value() = q.value(0).toDouble();
		}

		GET_VALUE(cap.R);
		GET_VALUE(cap.Ho);
		GET_VALUE(cap.Alpha);

		setCapillary(n, cap);

		progress_value += div_per_vessel;
		while (progress && progress_value - current_value > 1.0) {
			progress->setValue(progress->value()+1);
			current_value += 1.0;
			qApp->processEvents();
		}
	}


	// load disease stack
	QSqlQuery params(db);
	params.prepare("SELECT param_no, param_value FROM disease_param WHERE disease_id=?");

	q.prepare("SELECT disease_id, script FROM diseases");
	q.exec();
	dis.clear();
	while (q.next()) {
		int disease_id = q.value(0).toInt();
		Disease disease = Disease::fromString(q.value(1).toString());

		params.addBindValue(disease_id);
		params.exec();
		while (params.next())
			disease.setParameter(params.value(0).toInt(), params.value(1).toDouble());

		dis.push_back(disease);
		qApp->processEvents();
	}

	if (progress)
		progress->setValue(progress->value()+1);

	modified_flag = false;
	return true;
}

#undef GET_VALUE
#undef SET_VALUE

