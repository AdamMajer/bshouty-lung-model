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

/* No accuracy benefit above 128. Speed is not compromised at 128 (on 16 core machine!) */
const int nSums = 128; // number of divisions in the integral

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
	    !(significantChange(a.a, b.a) ||
	      significantChange(a.b, b.b) ||
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
	      significantChange(a.R, b.R));
}


// CONSTRUCTOR - always called - initializes everything
Model::Model(Transducer transducer_pos, ModelType type, int n_gen, IntegralType int_type)
{
	model_type = type;
	n_generations = n_gen;

	if (model_type == DoubleLung)
		n_generations++;

	vessel_value_override.resize(nElements()*2 + numCapillaries(), false);

	arteries = (Vessel*)allocateCachelineAligned(sizeof(Vessel)*numArteries());
	veins = (Vessel*)allocateCachelineAligned(sizeof(Vessel)*numVeins());
	caps = (Capillary*)allocateCachelineAligned(sizeof(Capillary)*numCapillaries());

	if (arteries == 0 || veins == 0 || caps == 0)
		throw std::bad_alloc();

	Krc_factor = calibrationValue(Krc);

	/* set to zero, mostly to prevent valgrind complaining */
	memset(arteries, 0, numArteries()*sizeof(Vessel));
	memset(veins, 0, numVeins()*sizeof(Vessel));
	memset(caps, 0, numCapillaries()*sizeof(Capillary));

	// Initial conditions
	trans_pos = transducer_pos;
	Tlrns = calibrationValue(Tlrns_value);

	PatHt = calibrationValue(Pat_Ht_value);
	PatWt = calibrationValue(Pat_Wt_value);
	LungHt = calibrationValue(Lung_Ht_value);

	Pal = calibrationValue(Pal_value);
	Ppl = calibrationValue(Ppl_value);

	CO = calibrationValue(CO_value);
	LAP = calibrationValue(LAP_value);

	Vrv = calibrationValue(Vrv_value);
	Vfrc = calibrationValue(Vfrc_value);
	Vtlc = calibrationValue(Vtlc_value);

	Hct = calibrationValue(Hct_value);
	PA_EVL = calibrationValue(PA_EVL_value);
	PA_diam = calibrationValue(PA_Diam_value);
	PV_EVL = calibrationValue(PV_EVL_value);
	PV_diam = calibrationValue(PV_Diam_value);

	CI = CO/BSAz();
	integral_type = int_type;

	// to have a default PAP value of 15
	arteries[0].flow = CO;
	arteries[0].total_R = (15.0-LAP)/arteries[0].flow;

	modified_flag = true;
	model_reset = true;
	abort_calculation = 0;

	bool opencl_helper = cl->isAvailable() &&
	                DbSettings::value(settings_opencl_enabled, true).toBool();
/*
	if (opencl_helper)
		integration_helper = new OpenCLIntegrationHelper(this);
	else
*/
		integration_helper = new CpuIntegrationHelper(
		                             this,
		                             integral_type);

	if (integration_helper == 0)
		throw std::bad_alloc();


	// Initial conditions
	initVesselBaselineCharacteristics();
}

Model::Model(const Model &other)
{
	n_generations = 0;
	*this = other;
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
	Tlrns = other.Tlrns;
	LungHt = other.LungHt;
	Pal = other.Pal;
	Ppl = other.Ppl;
	CO = other.CO;
	CI = other.CI;
	LAP = other.LAP;

	Vrv = other.Vrv;
	Vfrc = other.Vfrc;
	Vtlc = other.Vtlc;

	PatWt = other.PatWt;
	PatHt = other.PatHt;
	trans_pos = other.trans_pos;

	BSA_ratio = other.BSA_ratio;

	Hct = other.Hct;
	PA_EVL = other.PA_EVL;
	PA_diam = other.PA_diam;
	PV_EVL = other.PV_EVL;
	PV_diam = other.PV_diam;

	modified_flag = other.modified_flag;
	model_reset = other.model_reset;
	abort_calculation = other.abort_calculation;

	integral_type = other.integral_type;

	dis = other.dis;
	model_type = other.model_type;

	Krc_factor = other.Krc_factor;

	if (n_generations != other.n_generations) {
		/* If n_generations do not match, then we need to reallocate memory
		 * that store vessel information
		 */

		if (n_generations != 0) {
			// delete current memory allocations.
			// assume that n_generation == 0 is only from copy constructor
			freeAligned(arteries);
			freeAligned(veins);
			freeAligned(caps);
		}

		n_generations = other.n_generations;
		arteries = (Vessel*)allocateCachelineAligned(sizeof(Vessel)*numArteries());
		veins = (Vessel*)allocateCachelineAligned(sizeof(Vessel)*numVeins());
		caps = (Capillary*)allocateCachelineAligned(sizeof(Capillary)*numCapillaries());

		if (arteries == 0 || veins == 0 || caps == 0)
			throw std::bad_alloc();
	}

	memcpy(arteries, other.arteries, numArteries()*sizeof(Vessel));
	memcpy(veins, other.veins, numVeins()*sizeof(Vessel));
	memcpy(caps, other.caps, numCapillaries()*sizeof(Capillary));

	vessel_value_override = other.vessel_value_override;

	prog = other.prog;

	bool opencl_helper = cl->isAvailable() &&
	                DbSettings::value(settings_opencl_enabled, true).toBool();
	/*
	if (opencl_helper)
		integration_helper = new OpenCLIntegrationHelper(this);
	else
	*/
		integration_helper = new CpuIntegrationHelper(
		                             this,
		                             integral_type);

	if (integration_helper == 0)
		throw std::bad_alloc();

	return *this;
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

	const static int artery_number[16] = {
	        1,
	        2,
	        7,
	        43,
	        127,
	        450,
	        1724,
	        6225,
	        22004,
	        86020,
	        285772,
	        674169,
	        2256846,
	        5101903,
	        14057197,
	        51205812
	};

	const static int vein_number[16] = {
	        1,
	        2,
	        4,
	        11,
	        31,
	        63,
	        143,
	        435,
	        1067,
	        3721,
	        15567,
	        73025,
	        453051,
	        2166333,
	        8494245,
	        39823553
	};

	if (gen_no > (unsigned)nGenerations())
		return 0;

	switch (vessel_type) {
	case Vessel::Artery:
		return artery_number[gen_no-1];
	case Vessel::Vein:
		return vein_number[gen_no-1];
	}

	return 0;
}

double Model::measuredDiameterRatio(Vessel::Type vessel_type,
                                    unsigned gen_no)
{
	/* Values for the main artery and vein were added later assuming
	 * standard diameter ratio of 1.57
	 */
	const static double artery_ratios[16] = {
	        1,
		0.49664429530201,
		0.24630872483221,
		0.13959731543624,
		0.09093959731544,
		0.05872483221477,
		0.03892617449664,
		0.0258389261745,
		0.01711409395973,
		0.01140939597315,
		0.00738255033557,
		0.00503355704698,
		0.00325503355705,
		0.00187919463087,
		0.00120805369128,
		0.0006711409396
	};

	const static double vein_ratios[16] = {
	        1,
		0.67552083333333,
		0.45052083333333,
		0.30520833333333,
		0.20833333333333,
		0.15,
		0.10364583333333,
		0.07395833333333,
		0.046875,
		0.03229166666667,
		0.01979166666667,
		0.01197916666667,
		0.00677083333333,
		0.00348958333333,
		0.00161458333333,
		0.0009375
	};

	switch (vessel_type) {
	case Vessel::Artery:
		return artery_ratios[gen_no-1];
	case Vessel::Vein:
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
	const static double artery_ratios[16] = {
	        1,
	        0.506,
	        0.714,
	        0.5194,
	        0.3614,
	        0.247,
	        0.1316,
	        0.0746,
	        0.0562,
	        0.0384,
	        0.0216,
	        0.0136,
	        0.009,
	        0.0072,
	        0.0052,
	        0.0044
	};

	const static double vein_ratios[16] = {
	        1,
	        0.7136,
	        0.6998,
	        0.3898,
	        0.5298,
	        0.358,
	        0.2956,
	        0.2248,
	        0.1356,
	        0.0958,
	        0.0584,
	        0.03,
	        0.0212,
	        0.0076,
	        0.0042,
	        0.0026
	};

	switch (vessel_type) {
	case Vessel::Artery:
		return artery_ratios[gen_no-1];
	case Vessel::Vein:
		return vein_ratios[gen_no-1];
	}

	return 1;
}

/*
double Model::arteryResistanceFactor(int gen)
{
//	double r[] = {1, 3.31, 10.96, 36.29, 120.17};
//	return r[gen-1];
	if (art_calib.gen_r[0] == 0) {
		// calculate generation resistances for 16 generations
		const double len_ratio = art_calib.len_ratio;
		const double diam_ratio = art_calib.diam_ratio;
		const double branch_ratio = art_calib.branch_ratio;

		double a_factor = diam_ratio*diam_ratio*diam_ratio*diam_ratio / len_ratio;

		double n_vessel = 1;
		double ind_r = getKra();
		for (int i=0; i<16; ++i) {
			art_calib.gen_r[i] = ind_r * n_vessel;

			n_vessel *= 2.0 / branch_ratio;
			ind_r *= a_factor;
		}
	}
	return vesselResistanceFactor(gen, art_calib.gen_r);
}

double Model::veinResistanceFactor(int gen)
{
	if (vein_calib.gen_r[0] == 0) {
		// calculate generation resistances for 16 generations
		const double len_ratio = vein_calib.len_ratio;
		const double diam_ratio = vein_calib.diam_ratio;
		const double branch_ratio = vein_calib.branch_ratio;

		double v_factor = diam_ratio*diam_ratio*diam_ratio*diam_ratio / len_ratio;

		double n_vessel = 1;
		double ind_r = getKrv();
		for (int i=0; i<16; ++i) {
			vein_calib.gen_r[i] = ind_r * n_vessel;

			n_vessel *= 2.0 / branch_ratio;
			ind_r *= v_factor;
		}
	}

	return vesselResistanceFactor(gen, vein_calib.gen_r);
}

double Model::vesselResistanceFactor(int gen, const double *gen_r) const
{
	* Normally the remainder vesseks gets attached to the start of the
	 * first generation unless the remainder is larger than number of vessels
	 * in a generation, then number of vessels per generation is increased by 1
	 * and remaing vessels get grouped at beginning.. The goal is,
	 *
	 *  15 gen => div=1, rem=1
	 *  6 gen => div=3, rem=1
	 *  5 gen => div=3, rem=1
	 *
	int div = 16 / n_generations;
	int rem = 16 % n_generations;

	int start_gen_idx, end_gen_idx;
	if (rem*n_generations >= 16) {
		// Remainder gets is separated at first generation
		div = 16/(n_generations-1);
		rem = 16%(n_generations-1);

		if (gen == 1) {
			start_gen_idx = 0;
			end_gen_idx = rem;
		}
		else {
			start_gen_idx = div*(gen-2)+rem;
			end_gen_idx = div*(gen-1)+rem;
		}
	}
	else {
		// Remainder gets attached to first generation
		if (gen == 1) {
			start_gen_idx = 0;
			end_gen_idx = div+rem;
		}
		else {
			start_gen_idx = div*(gen-1)+rem;
			end_gen_idx = div*gen+rem;
		}
	}


	double gen_sum=0;

	// Calculating individual model resistances for the target generation
	for (int i=start_gen_idx; i<end_gen_idx; ++i)
		gen_sum += gen_r[i] / nElements(i+1);

	return gen_sum * nElements(gen);
}
*/

double Model::BSAz() const
{
	return BSA(calibrationValue(Model::Pat_Ht_value),
	           calibrationValue(Model::Pat_Wt_value));
}

double Model::BSA(double pat_ht, double pat_wt)
{
	 return sqrt(pat_ht * pat_wt) / 60.0;
}

const Vessel& Model::artery( int gen, int index ) const
{
	if( gen <= 0 || index < 0 || gen > n_generations || index >= nElements( gen ))
		throw "Out of bounds";

	return arteries[ index + startIndex( gen )];
}

const Vessel& Model::vein( int gen, int index ) const
{
	if( gen <= 0 || index < 0 || gen > n_generations || index >= nElements( gen ))
		throw "Out of bounds";

	return veins[ index + startIndex( gen )];
}

const Capillary& Model::capillary( int index ) const
{
	if( index < 0 || index >= nElements( n_generations ))
		throw "Out of bounds";

	return caps[ index ];
}

void Model::setArtery(int gen, int index, const Vessel & v, bool override)
{
	if( gen <= 0 || index < 0 || gen > n_generations || index >= nElements( gen ))
		throw "Out of bounds";

	const int idx = index + startIndex(gen);

	arteries[idx] = v;
	vessel_value_override[idx] = vessel_value_override[idx] || override;
	modified_flag = true;
}

void Model::setVein(int gen, int index, const Vessel & v, bool override)
{
	if( gen <= 0 || index < 0 || gen > n_generations || index >= nElements( gen ))
		throw "Out of bounds";

	const int idx = index + startIndex(gen);
	const int o_idx = nElements() + idx;

	veins[idx] = v;
	vessel_value_override[o_idx] = vessel_value_override[o_idx] || override;
	modified_flag = true;
}

void Model::setCapillary(int index, const Capillary & c, bool override)
{
	if( index < 0 || index >= nElements( n_generations ))
		throw "Out of bounds";

	const int c_idx = index + nElements()*2;

	caps[ index ] = c;
	vessel_value_override[c_idx] = vessel_value_override[c_idx] || override;
	modified_flag = true;
}

double Model::getResult(DataType type) const
{
	switch( type ){
	case Lung_Ht_value:
		return LungHt;
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
		return LAP + arteries[0].flow * arteries[0].total_R;
	case Rus_value:
		return (getResult(PAP_value) - getResult(PciA_value)) / CO;
	case Rds_value:
		return (getResult(PcoA_value) - LAP) / CO;
	case Rm_value:
		return (getResult(PciA_value) - getResult(PcoA_value)) / CO;
	case Rt_value:
		return (getResult(PAP_value) - LAP) / CO;
	case PciA_value:{
			double ret = 0.0;
			int n = nElements( n_generations );
			int start = startIndex( n_generations );
			int end = start + n;
			while( start < end ){
				if (isnan(arteries[start].pressure_out))
					n--;
				else
					ret += arteries[start].pressure_out;
				start++;
			}

			return ret / n;
		}
	case PcoA_value:{
			double ret = 0.0;
			int n = nElements( n_generations );
			int start = startIndex( n_generations );
			int end = start + n;
			while( start < end ){
				if (isnan(veins[start].pressure_in))
					n--;
				else
					ret += veins[start].pressure_in;
				start++;
			}

			return ret / n;
		}
	case Tlrns_value:
		return Tlrns;
	case Pat_Ht_value:
		return PatHt;
	case Pat_Wt_value:
		return PatWt;
	case TotalR_value:
		return arteries[0].total_R;
	case DiseaseParam:
		break;

	case Vrv_value:
		return Vrv * 100.0;
	case Vfrc_value:
		return Vfrc * 100.0;
	case Vtlc_value:
		return Vtlc * 100.0;

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

	case Krc:
		return Krc_factor;
	}

	// Handle special case of DiseaseParam
	if ((type & DiseaseParam) == DiseaseParam) {
		const int dis_no = diseaseNo(type);
		const int param_no = diseaseParamNo(type);

		if (dis_no < dis.size())
			return dis.at(dis_no).parameterValue(param_no);
	}

	throw "Should never get here";
}

bool Model::setData(DataType type, double val)
{
	switch( type ){
	case Lung_Ht_value:
		if (significantChange(LungHt, val)) {
			LungHt = val;
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
	case Vrv_value:
		val /= 100.0;
		if (significantChange(Vrv, val)) {
			Vrv = val;
			calculateBaselineCharacteristics();
			modified_flag = true;
			return true;
		}
		break;
	case Vfrc_value:
		val /= 100.0;
		if (significantChange(Vfrc, val)) {
			Vfrc = val;
			calculateBaselineCharacteristics();
			modified_flag = true;
			return true;
		}
		break;
	case Vtlc_value:
		val /= 100.0;
		if (significantChange(Vtlc, val)) {
			Vtlc = val;
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
	case Ptp_value:
	case PAP_value:
	case Rus_value:
	case Rds_value:
	case Rm_value:
	case Rt_value:
	case PciA_value:
	case PcoA_value:
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

			double baseline_diameter = 4.85 + 13.43*sqrt(BSA(PatHt, PatWt));
			double new_diameter = 4.85 + 13.43*sqrt(BSA(val, PatWt));

			PatHt = val;
			PA_diam *= new_diameter/baseline_diameter;
			PV_diam *= new_diameter/baseline_diameter;

			initVesselBaselineResistances();
			modified_flag = true;
			return true;
		}
		break;
	case Pat_Wt_value:
		if (significantChange(PatWt, val)) {
			double baseline_diameter = 4.85 + 13.43*sqrt(BSA(PatHt, PatWt));
			double new_diameter = 4.85 + 13.43*sqrt(BSA(PatHt, val));

			PatWt = val;
			PA_diam *= new_diameter/baseline_diameter;
			PV_diam *= new_diameter/baseline_diameter;

			initVesselBaselineResistances();
			modified_flag = true;
			return true;
		}
		break;
	case DiseaseParam:
		break;

	case Krc:
		setKrFactors(val);
		break;
	}

	// Handle special case of DiseaseParam
	if ((type & DiseaseParam) == DiseaseParam) {
		const int dis_no = diseaseNo(type);
		const int param_no = diseaseParamNo(type);

		if (dis_no < dis.size()) {
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

int Model::calc( int max_iter )
{
	abort_calculation = 0;
	if (model_reset) {
		getParameters();
		for (DiseaseList::iterator i=dis.begin(); i!=dis.end(); ++i)
			i->processModel(*this);

		model_reset = false;
	}

	int i=0;
	do {
		vascPress();

		int iter_prog = 10000*i/max_iter;
		if (prog < iter_prog)
			prog = iter_prog;
	}while( !deltaR() &&
	                ++i < max_iter &&
	                abort_calculation==0);

	modified_flag = true;
	return i;
}

bool Model::calculationErrors() const
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
			case Model::Vrv_value:
			case Model::Vfrc_value:
			case Model::Vtlc_value:
			case Model::Hct_value:
				return ret_value / 100.0;
			default:
				return ret_value;
			}
		}
	}


	switch (type) {
	case Model::Tlrns_value:
		return 0.0001;
	case Model::Pat_Ht_value:
		return 175.0;
	case Model::Pat_Wt_value:
		return 75.0;
	case Model::Lung_Ht_value:
		return 20;
	case Model::Vrv_value:
		return 0.3;
	case Model::Vfrc_value:
		return 0.45;
	case Model::Vtlc_value:
		return 1.0;
	case Model::CO_value:
		return 6.45;
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
		return 3.115307890;
	case Model::PV_Diam_value:
		return 1.681263294;

	case Model::Krc:
		return 3179.855937396;

	case Model::Ptp_value:
	case Model::PAP_value:
	case Model::Rus_value:
	case Model::Rds_value:
	case Model::Rm_value:
	case Model::Rt_value:
	case Model::PciA_value:
	case Model::PcoA_value:
	case Model::TotalR_value:
	case Model::DiseaseParam:
		return std::numeric_limits<double>::quiet_NaN();
	}

	return 0.0;
}

void Model::getParameters()
{
	/* This procedure gets the arterial and venous parameters
	 * that determine perivascular pressure
	 *
	 * Adjust length basedon Ptp
	 */
	int n = nElements();
	for (int i=0; i<n; i++) {
		Vessel &art = arteries[i];
		art.perivascular_press_a = 15.0 - 0.2*art.Ptp;
		art.perivascular_press_b = -16.0 - 0.2*art.Ptp;
		art.perivascular_press_c = -0.013 - 0.0002*art.Ptp;

		art.length *= art.length_factor;


		Vessel &vein = veins[i];
		vein.perivascular_press_a = 12.0 - 0.1*vein.Ptp;
		vein.perivascular_press_b = -12.0 - 0.2*vein.Ptp;
		vein.perivascular_press_c = -0.020 - 0.0002*vein.Ptp;

		vein.length *= vein.length_factor;
	}
}

void Model::vascPress()
{
	// Calculate total resistances for each element
	totalResistance( 0 );

	// Calculate Flow (Q) and then Pressure (P) for each resistance
	veins[0].flow = CO;
	arteries[0].flow = CO;

	double PAP = LAP + arteries[0].flow * arteries[0].total_R;
	arteries[0].pressure_in = PAP;
	arteries[0].pressure_out = PAP - arteries[0].flow * arteries[0].R;
	veins[0].pressure_in = LAP + veins[0].flow * veins[0].R;
	veins[0].pressure_out = LAP;

	calculateChildrenFlowPress( 0 );
}


double Model::totalResistance(int i)
{
	// get current generation and determine if this is final generation
	int gen = gen_no( i );
	if( gen == nGenerations()){
		double R = arteries[i].R + caps[i-startIndex(gen)].R + veins[i].R;
		arteries[i].total_R = R;
		veins[i].total_R = R;

		return R;
	}

	// not the final generation, determine connection indexes
	int current_gen_start = startIndex( gen );
	int next_gen_start = startIndex( gen+1 );

	int connection_first = (i - current_gen_start) * 2 + next_gen_start;

	// Add the connecting resistances in parallel
	// TODO: parallelize
	const double R1 = totalResistance(connection_first);
	const double R2 = totalResistance(connection_first+1);
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

void Model::calculateChildrenFlowPress( int i )
{
	int gen = gen_no( i );

	// last generation, no children
	if( gen == nGenerations())
		return;

	// determine connecting indexes
	int current_gen_start = startIndex( gen );
	int next_gen_start = startIndex( gen+1 );

	int connection_first = ( i - current_gen_start ) * 2 + next_gen_start;

	// Calculate flow and pressure in children vessels based on the calculated flow and their resistances
	for( int con=connection_first; con<=connection_first+1; con++ ){
		if (isinf(arteries[con].R) || isinf(veins[con].R)) {
			// all vessels inside have no flow, so pressure is undefined
			arteries[con].pressure_in = arteries[i].pressure_out - (arteries[con].GP - arteries[i].GP)/1.35951002636;
			veins[con].pressure_out = veins[i].pressure_in - (veins[con].GP - veins[i].GP)/1.35951002636;

			arteries[con].flow = 0.0;
			veins[con].flow = 0.0;

			arteries[con].pressure_out = std::numeric_limits<double>::quiet_NaN();
			veins[con].pressure_in = std::numeric_limits<double>::quiet_NaN();
			veins[con].pressure_out = veins[i].pressure_in + (veins[con].GP - veins[i].GP)/1.35951002636;
		}

		else {
			arteries[con].pressure_in = arteries[i].pressure_out - (arteries[con].GP - arteries[i].GP)/1.35951002636;
			veins[con].pressure_out = veins[i].pressure_in - (veins[con].GP - veins[i].GP)/1.35951002636;

			const double flow =  (arteries[con].pressure_in - veins[con].pressure_out) /
			                     arteries[con].total_R;
			veins[con].flow = flow;
			arteries[con].flow = flow;

			arteries[con].pressure_out = arteries[con].pressure_in - arteries[con].flow * arteries[con].R;
			veins[con].pressure_in = veins[con].pressure_out + veins[con].flow * veins[con].R;
		}
	}

	// Now, calculate the same thing for child generations
	// TODO: parallelize
	calculateChildrenFlowPress( connection_first );
	calculateChildrenFlowPress( connection_first+1 );
}

double Model::deltaCapillaryResistance( int i )
{
	const int gen_start = startIndex( nGenerations());
	const Vessel & con_artery = arteries[gen_start+i];
	const Vessel & con_vein = veins[gen_start+i];

	if (isnan(con_artery.pressure_out) || isnan(con_vein.pressure_in)) {
		// pressure is undefined, no flow
		return 0;
	}

	const double Ri = caps[i].R;
	const double Pin = 1.35951002636 * con_artery.pressure_out; // convert pressures from mmHg => cmH20
	const double Pout = 1.35951002636 * con_vein.pressure_in;
	Capillary & cap = caps[i];

/*
	if (Pin-Pout < Pin*1e6) {
		cap.R = 0.0;
		return Ri==0.0 ? 0.0 : 1.0;
	}
*/

	const double x = Pout - Pal;
	const double y = Pin - Pal;
	const double FACC = y-x;
	const double FACC1 = 25.0-x;
	const double Rz = getKrc() * BSA_ratio * nElements(n_generations) / nElements(16);

	if( x < 0 ){
		if( y < 0 )
			cap.R = y/con_artery.flow - x/con_artery.flow;
		else if( y < 25 )
			cap.R = FACC * Rz /( 0.001229*( exp( 4*log(cap.Ho + cap.Alpha*y ))-exp( 4*log( cap.Ho ))));
		else // y >= 25
			cap.R = (1-x/y) * 25*Rz/(0.001229*(exp(4*log(cap.Ho+cap.Alpha*25))-exp(4*log(cap.Ho))));
	}
	else if( x < 25 ){
		if( y < 0 )
			throw "Internal capillary resistance error 1";
		else if( y < 25 )
			cap.R = FACC*Rz/(0.001229*( exp( 4*log( cap.Ho+cap.Alpha*y ))-exp( 4*log( cap.Ho+cap.Alpha*x ))));
		else // y >= 25
			cap.R = FACC1*Rz/(0.001229*( exp(4*log( cap.Ho+cap.Alpha*25 ))-exp( 4*log( cap.Ho+cap.Alpha*x))));
	}
	else { // x >= 25
		cap.R = Rz;

		if( y < 25 )
			throw "Internal capillary resistance error 2";
	}


	if (Ri == 0.0) // avoid dividing by 0 and return pressure gradient instead
		return FACC;

	return fabs(cap.R-Ri)/Ri; // return different from target tolerance
}

bool Model::deltaR()
{
	// integrate resistance of veins and arteries
	QFutureSynchronizer<double> results;

	double max_deviation = integration_helper->integrate();

	// calculate resistances of capillaries
	int n = nElements( nGenerations());
	QVector<int> v(n);
	for( int i=0; i<n; i++ ) {
		v[i] = i;
		results.addFuture(QtConcurrent::run(this, &Model::deltaCapillaryResistance, v[i]));
	}

	const QList<QFuture<double> > &result_futures = results.futures();
	foreach (const QFuture<double> &i, result_futures)
		max_deviation = std::max(i.result(), max_deviation);

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

		// correcting area to Ptp=0, Ptm=35 cmH2O
		//arteries[i].a = 0.2419 / 1.2045;
		arteries[i].vessel_outside_lung = isOutsideLung(i);
		arteries[i].a = 2.0;
		arteries[i].b = 8.95866; //0.0275 / 1.2045;
		arteries[i].c = -0.060544062342;
		arteries[i].tone = 0;

		arteries[i].vessel_ratio = static_cast<double>(nElements(gen_no(i))) /
		                        nVessels(Vessel::Artery, gen_no(i));
	}

	const int num_veins = numVeins();
	for (int i=0; i<num_veins; ++i) {
		if (vessel_value_override[num_arteries + i])
			continue;

		if (isOutsideLung(i)) {
			// correct for vessels outside the lung
			veins[i].vessel_outside_lung = 1;
			veins[i].b = 10.0;
			veins[i].c = -10.0;
		}
		else {
			// correcting area to Ptp=0, Ptm=35 cmH2O
			veins[i].vessel_outside_lung = 0;
			veins[i].b = 2.4307;
			veins[i].c = -0.2355;
		}
		veins[i].a = 1.0;
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
		if (vessel_value_override[nElements()*2 + i])
			continue;

		caps[i].R = cKrc * nElements(n_generations) / nElements(16);
	}
}

void Model::initVesselBaselineResistances(int gen)
{
	// Get starting index and number of elements in the generation
	int start_index = startIndex( gen );
	int num_elements = nElements( gen );
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

		if (!vessel_value_override[n_arteries+i]) {
			const double vein_d = 1e4 * PV_diam * measuredDiameterRatio(Vessel::Vein, gen);
			veins[i].length = 1e4 * PV_EVL * measuredLengthRatio(Vessel::Vein, gen);
			veins[i].D = vein_d;
			veins[i].R = Krv_factor*veins[i].length/sqr(sqr(vein_d));
			veins[i].viscosity_factor = 1.0;
			veins[i].volume = 1e-9 * M_PI/4.0*vein_d*vein_d*veins[i].length / vein_ratio;
		}

		// GP was calculated with GP=0 being top of lung
		// then corrected based on transducer position
		// GPz is a fraction of lung height
		int gp_gen = gen;
		int effective_ngen = n_generations;

		switch (model_type) {
		case SingleLung:
			break;
		case DoubleLung:
			effective_ngen--;
			if (gen > 1) {
				gp_gen--;
				vessel_no %= nElements(gp_gen);
			}
		}

		const double GPz = (vessel_no*exp((effective_ngen-gp_gen+1)*M_LN2)+
		                 exp((effective_ngen-gp_gen)*M_LN2)-1) /
		                (exp((effective_ngen)*M_LN2) - 2);

		if (!vessel_value_override[i])
			arteries[i].GPz = GPz;
		if (!vessel_value_override[n_arteries+i])
			veins[i].GPz = GPz;
	}

	// Initialize more generations, if they exist
	if( gen < nGenerations())
		initVesselBaselineResistances(gen+1);
}

void Model::calculateBaselineCharacteristics()
{
	/* This function corrects for transducer position and
	 * calculates vessel Ppl and vessel Ptp based on transducer position
	 * as well as global Ppl and Pal
	 */

	const int n = nElements();
	for (int i=0; i<n; ++i) {
		Vessel &art = arteries[i];
		Vessel &vein = veins[i];

		switch (trans_pos) {
		case Top:
			art.GP = -art.GPz*LungHt;
			vein.GP = -vein.GPz*LungHt;
			break;
		case Middle:
			art.GP = LungHt/2 - art.GPz*LungHt;
			vein.GP = LungHt/2 - vein.GPz*LungHt;
			break;
		case Bottom:
			art.GP = LungHt-art.GPz*LungHt;
			vein.GP = LungHt-vein.GPz*LungHt;
			break;
		}

		// for every 1 cmH2O GP, Ppl changes by 0.55
		art.Ppl = Ppl - 0.55*art.GP;
		art.Ptp = Pal - art.Ppl;
		vein.Ppl = Ppl - 0.55*vein.GP;
		vein.Ptp = Pal - vein.Ppl;

		if (art.Ptp < 0)
			art.Ptp = 0;
		if (vein.Ptp < 0)
			vein.Ptp = 0;

		if (isOutsideLung(i)) {
			art.length_factor = 1.0;
			vein.length_factor = 1.0;
		}
		else {
			art.length_factor = lengthFactor(art);
			vein.length_factor = lengthFactor(vein);
		}
	}

	// Initialize capillaries
	const int num_capillaries = numCapillaries();
	const int num_arteries = numArteries();
	const int num_veins = numVeins();
	const int start_offset = startIndex(n_generations);
	for (int i=0; i<num_capillaries; i++) {
		const Vessel &connected_vessel = arteries[i+start_offset];
		if (vessel_value_override[num_arteries + num_veins + i])
			continue;

		double cap_ppl = Ppl - 0.55*connected_vessel.GP;
		double cap_ptp = Pal - cap_ppl;
		caps[i].Alpha = 0.073+0.2*exp(-0.141*cap_ptp);
		caps[i].Ho = 2.5;
	}
}

double Model::lengthFactor(const Vessel &v) const
{
	const double min_ratio = 0.4 * Vtlc;
	double len_ratio;

	if (v.Ptp > 35.0)
		return 1.0;

	if (v.Ptp > 15.0)
		len_ratio = 0.005 * v.Ptp + 0.825;
	else if (v.Ptp > 5.0)
		len_ratio = (0.09-0.1*Vfrc)*v.Ptp + 1.5*Vfrc - 0.45;
	else
		len_ratio = 0.2*(Vfrc-Vrv)*v.Ptp + Vrv;

	len_ratio = qMax(len_ratio, min_ratio);
	return cbrt(len_ratio);
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
	SET_VALUE(LungHt);
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

	SET_VALUE(Vrv);
	SET_VALUE(Vfrc);
	SET_VALUE(Vtlc);

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

	q.addBindValue("model_type");
	q.addBindValue(offset);
	q.addBindValue((int)model_type);
	q.exec();

	q.addBindValue("ngen");
	q.addBindValue(offset);
	q.addBindValue(n_generations);
	q.exec();

	q.addBindValue("integral_type");
	q.addBindValue(offset);
	q.addBindValue((int)integral_type);
	q.exec();

	if (progress)
		progress->setValue(progress->value()+1);

	double div_per_vessel = (nElements()*2+nElements(n_generations)) * 0.999 / 998.0;
	double progress_value=0, current_value=0;

	// save each vessel value
	values.clear();

	q.exec("DELETE FROM vessel_values WHERE offset=" + QString::number(offset));
	q.prepare("INSERT INTO vessel_values (type, vessel_idx, key, offset, value) "
	          "VALUES (?, ?, ?, ?, ?)");
	int n_elements = nElements();
	for (int type=1; type<=2; ++type) {
		for (int n=0; n<n_elements; ++n) {
			const int override_offset = (type==1 ? n : nElements()+n);
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

			SET_VALUE(v.a);
			SET_VALUE(v.b);
			SET_VALUE(v.c);
			SET_VALUE(v.tone);

			SET_VALUE(v.GP);
			SET_VALUE(v.GPz);
			SET_VALUE(v.Ppl);
			SET_VALUE(v.Ptp);

			SET_VALUE(v.perivascular_press_a);
			SET_VALUE(v.perivascular_press_b);
			SET_VALUE(v.perivascular_press_c);

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

	n_elements = nElements(n_generations);
	values.clear();

	for (int n=0; n<n_elements; ++n) {
		const int override_offset = nElements()*2+n;
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
	SET_VALUE(LungHt);
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

	SET_VALUE(Vrv);
	SET_VALUE(Vfrc);
	SET_VALUE(Vtlc);

	SET_VALUE(Krc_factor);

	BSA_ratio = BSAz() / BSA(PatWt, PatHt);

	q.prepare("SELECT value FROM model_values WHERE key = ? AND offset = ?");

	/* Load transducer and model_type first and make certain the model has correct
	 * number of generations of vessels allocated.
	 */
	q.addBindValue("transducer");
	q.addBindValue(offset);
	if (!q.exec() || !q.next()) {
		qDebug() << q.lastError().text();
		return false;
	}
	trans_pos = (Transducer)q.value(0).toInt();

	q.addBindValue("model_type");
	q.addBindValue(offset);
	if (!q.exec() || !q.next())
		model_type = SingleLung;
	else
		model_type = (ModelType)q.value(0).toInt();

	q.addBindValue("integral_type");
	q.addBindValue(offset);
	if (!q.exec() || !q.next())
		integral_type = BshoutyIntegral;
	else
		integral_type = (IntegralType)q.value(0).toInt();

	q.addBindValue("ngen");
	q.addBindValue(offset);
	if (!q.exec() || !q.next())
		return false;
	int new_gen = q.value(0).toInt();
	if (new_gen == 0)
		return false;
	if (new_gen != nGenerations()) {
		if (model_type == DoubleLung)
			new_gen--;
		*this = Model(trans_pos, model_type, new_gen, integral_type);
	}

	// Load all values
	for (QMap<QString,double>::iterator i=values.begin(); i!=values.end(); ++i) {
		q.addBindValue(i.key());
		q.addBindValue(offset);
		if (!q.exec() || !q.next())
			return false;

		i.value() = q.value(0).toDouble();
	}

	GET_VALUE(Tlrns);
	GET_VALUE(LungHt);
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

	GET_VALUE(Vrv);
	GET_VALUE(Vfrc);
	GET_VALUE(Vtlc);

	GET_VALUE(Krc_factor);

	// force recalculation of Kr factors
	setKrFactors(Krc_factor);

	// load values of each vessel
	values.clear();

	if (progress)
		progress->setValue(progress->value()+1);

	double div_per_vessel = 998.0 / (nElements()*2+nElements(n_generations));
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
			const int override_offset = (type==1 ? n : nElements()+n);
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

			SET_VALUE(v.a);
			SET_VALUE(v.b);
			SET_VALUE(v.c);
			SET_VALUE(v.tone);

			SET_VALUE(v.GP);
			SET_VALUE(v.GPz);
			SET_VALUE(v.Ppl);
			SET_VALUE(v.Ptp);

			SET_VALUE(v.perivascular_press_a);
			SET_VALUE(v.perivascular_press_b);
			SET_VALUE(v.perivascular_press_c);

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

			GET_VALUE(v.a);
			GET_VALUE(v.b);
			GET_VALUE(v.c);
			GET_VALUE(v.tone);

			GET_VALUE(v.GP);
			GET_VALUE(v.GPz);
			GET_VALUE(v.Ppl);
			GET_VALUE(v.Ptp);

			GET_VALUE(v.perivascular_press_a);
			GET_VALUE(v.perivascular_press_b);
			GET_VALUE(v.perivascular_press_c);

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
		Capillary &cap = caps[n];

		vessel_value_override[nElements()*2+n] = true;

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
		Disease disease(q.value(1).toString());

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

