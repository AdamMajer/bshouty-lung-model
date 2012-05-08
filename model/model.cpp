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

/* NOTE: Changing these will require changing these values in OpenCL sources!!
*/
const double K1 = 0.5;
const double K2 = 0.5;
const int nSums = 10000; // number of divisions in the integral

/* Helper functions */
static inline double sqr( double n )
{
	return n*n;
}

static inline bool significantChange(double old_val, double new_val)
{
	/* Returns true if old_val differs from new_val by more than 1 part in 10e6 */
	double abs_sum = fabs(old_val + new_val);

	if (abs_sum < 10e-6)
		return true;

	return fabs(old_val - new_val) / abs_sum > 10e-6;
}


bool operator==(const struct Vessel &a, const struct Vessel &b)
{
	return memcmp(&a, &b, sizeof(struct Vessel)) == 0 ||
	    !(significantChange(a.a, b.a) ||
	      significantChange(a.b, b.b) ||
	      significantChange(a.c, b.c) ||
	      significantChange(a.R, b.R) ||
	      significantChange(a.tone, b.tone) ||
	      significantChange(a.GP, b.GP) ||
	      significantChange(a.GPz, b.GPz) ||
	      significantChange(a.Ppl, b.Ppl) ||
	      significantChange(a.Ptp, b.Ptp) ||
	      significantChange(a.perivascular_press_a, b.perivascular_press_a) ||
	      significantChange(a.perivascular_press_b, b.perivascular_press_b) ||
	      significantChange(a.perivascular_press_c, b.perivascular_press_c) ||
	      significantChange(a.length_factor, b.length_factor) ||
	      significantChange(a.Kz, b.Kz) ||
	      significantChange(a.total_R, b.total_R) ||
	      significantChange(a.pressure, b.pressure) ||
	      significantChange(a.flow, b.flow));
}

bool operator==(const struct Capillary &a, const struct Capillary &b)
{
	return memcmp(&a, &b, sizeof(struct Capillary)) == 0 ||
	    !(significantChange(a.Alpha, b.Alpha) ||
	      significantChange(a.Ho, b.Ho) ||
	      significantChange(a.R, b.R));
}

CalibrationFactors::CalibrationFactors(CalibrationType type)
{
	switch (type) {
	case Artery:
		branch_ratio = DbSettings::value(art_branching_ratio, 3.36).toDouble();
		len_ratio = DbSettings::value(art_length_ratio, 1.49).toDouble();
		diam_ratio = DbSettings::value(art_diam_ratio, 1.56).toDouble();
		break;
	case Vein:
		branch_ratio = DbSettings::value(vein_branching_ratio, 3.33).toDouble();
		len_ratio = DbSettings::value(vein_length_ratio, 1.50).toDouble();
		diam_ratio = DbSettings::value(vein_diam_ratio, 1.58).toDouble();
		break;
	}

	memset(gen_r, 0, sizeof(double)*16);
}


// CONSTRUCTOR - always called - initializes everything
Model::Model(Transducer transducer_pos, ModelType type, int n_gen)
        : art_calib(CalibrationFactors::Artery),
          vein_calib(CalibrationFactors::Vein)
{
	model_type = type;
	n_generations = n_gen;

	if (model_type == DoubleLung)
		n_generations++;

	arteries = (Vessel*)allocateCachelineAligned(sizeof(Vessel)*numArteries());
	veins = (Vessel*)allocateCachelineAligned(sizeof(Vessel)*numVeins());
	caps = (Capillary*)allocateCachelineAligned(sizeof(Capillary)*numCapillaries());

	if (arteries == 0 || veins == 0 || caps == 0)
		throw std::bad_alloc();

	Kra_factor = calibrationValue(Kra);
	Krv_factor = calibrationValue(Krv);
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
	MV = calibrationValue(MV_value);
	CL = calibrationValue(CL_value);

	Pal = calibrationValue(Pal_value);
	Ppl = calibrationValue(Ppl_value);

	CO = calibrationValue(CO_value);
	LAP = calibrationValue(LAP_value);

	CI = CO/BSAz();

	// to have a default PAP value of 15
	arteries[0].flow = CO;
	arteries[0].total_R = (15.0-LAP)/arteries[0].flow;

	modified_flag = true;
	model_reset = true;
	abort_calculation = 0;

	bool opencl_helper = cl->isAvailable() &&
	                DbSettings::value(settings_opencl_enabled, true).toBool();
	if (opencl_helper)
		integration_helper = new OpenCLIntegrationHelper(this);
	else
		integration_helper = new CpuIntegrationHelper(this);

	if (integration_helper == 0)
		throw std::bad_alloc();


	// Initial conditions
	initVesselBaselineCharacteristics();
}

Model::Model(const Model &other)
        : art_calib(other.art_calib),
          vein_calib(other.vein_calib)
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
	MV = other.MV;
	CL = other.CL;
	Pal = other.Pal;
	Ppl = other.Ppl;
	CO = other.CO;
	CI = other.CI;
	LAP = other.LAP;

	PatWt = other.PatWt;
	PatHt = other.PatHt;
	trans_pos = other.trans_pos;

	modified_flag = other.modified_flag;
	model_reset = other.model_reset;
	abort_calculation = other.abort_calculation;

	dis = other.dis;
	model_type = other.model_type;

	Kra_factor = other.Kra_factor;
	Krv_factor = other.Krv_factor;
	Krc_factor = other.Krc_factor;
	art_calib = other.art_calib;
	vein_calib = other.vein_calib;

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

	prog = other.prog;

	bool opencl_helper = cl->isAvailable() &&
	                DbSettings::value(settings_opencl_enabled, true).toBool();
	if (opencl_helper)
		integration_helper = new OpenCLIntegrationHelper(this);
	else
		integration_helper = new CpuIntegrationHelper(this);

	if (integration_helper == 0)
		throw std::bad_alloc();

	return *this;
}

Model* Model::clone() const
{
	return new Model(*this);
}

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
	/* Normally the remainder vesseks gets attached to the start of the
	 * first generation unless the remainder is larger than number of vessels
	 * in a generation, then number of vessels per generation is increased by 1
	 * and remaing vessels get grouped at beginning.. The goal is,
	 *
	 *  15 gen => div=1, rem=1
	 *  6 gen => div=3, rem=1
	 *  5 gen => div=3, rem=1
	 */
	int div = 16 / n_generations;
	int rem = 16 % n_generations;

	int start_gen_idx, end_gen_idx;
	if (rem*n_generations >= 16) {
		/* Remainder gets is separated at first generation */
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
		/* Remainder gets attached to first generation */
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

void Model::setArtery( int gen, int index, const Vessel & v )
{
	if( gen <= 0 || index < 0 || gen > n_generations || index >= nElements( gen ))
		throw "Out of bounds";

	arteries[ index + startIndex( gen )] = v;
	modified_flag = true;
}

void Model::setVein( int gen, int index, const Vessel & v )
{
	if( gen <= 0 || index < 0 || gen > n_generations || index >= nElements( gen ))
		throw "Out of bounds";

	veins[ index + startIndex( gen )] = v;
	modified_flag = true;
}

void Model::setCapillary( int index, const Capillary & c )
{
	if( index < 0 || index >= nElements( n_generations ))
		throw "Out of bounds";

	caps[ index ] = c;
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
				ret += arteries[start].pressure;
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
				ret += veins[start].pressure;
				start++;
			}

			return ret / n;
		}
	case Tlrns_value:
		return Tlrns;
	case MV_value:
		return MV;
	case CL_value:
		return CL;
	case Pat_Ht_value:
		return PatHt;
	case Pat_Wt_value:
		return PatWt;
	case TotalR_value:
		return arteries[0].total_R;
	case DiseaseParam:
		break;

	case Kra:
		return Kra_factor;
	case Krv:
		return Krv_factor;
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
	case MV_value:
		if (significantChange(MV, val)) {
			setNewVesselMV(MV, val);
			MV = val;
			modified_flag = true;
			return true;
		}
		break;
	case CL_value:
		if (significantChange(CL, val)) {
			setNewVesselCL(CL, val);
			CL = val;
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
			PatHt = val;
			initVesselBaselineResistances();
			modified_flag = true;
			return true;
		}
		break;
	case Pat_Wt_value:
		if (significantChange(PatWt, val)) {
			PatWt = val;
			initVesselBaselineResistances();
			modified_flag = true;
			return true;
		}
		break;
	case DiseaseParam:
		break;

	case Kra:
		setKrFactors(val, Krv_factor, Krc_factor);
		break;
	case Krv:
		setKrFactors(Kra_factor, val, Krc_factor);
		break;
	case Krc:
		setKrFactors(Kra_factor, Krc_factor, val);
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
		getKz();

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

void Model::setKrFactors(double Kra, double Krv, double Krc)
{
	Kra_factor = Kra;
	Krv_factor = Krv;
	Krc_factor = Krc;

	// flag recalculation of gen_r
	memset(art_calib.gen_r, 0, sizeof(double)*16);
	memset(vein_calib.gen_r, 0, sizeof(double)*16);
}

double Model::getKra()
{
	if (Kra_factor == 0.0)
		Kra_factor = calibrationValue(Kra);

	return Kra_factor;
}

double Model::getKrv()
{
	if (Krv_factor == 0.0)
		Krv_factor = calibrationValue(Krv);

	return Krv_factor;
}

double Model::getKrc()
{
	if (Krc_factor == 0.0)
		Krc_factor = calibrationValue(Krc);

	return Krc_factor;
}

void Model::setCalibrationRatios(const CalibrationFactors &a, const CalibrationFactors &v)
{
	art_calib = a;
	vein_calib = v;

	// flag recalculation of gen_r
	memset(art_calib.gen_r, 0, sizeof(double)*16);
	memset(vein_calib.gen_r, 0, sizeof(double)*16);
}

QString Model::calibrationPath(DataType type)
{
	return "/settings/calibration/" + QString::number(type);
}

double Model::calibrationValue(DataType type)
{
	QVariant ret = DbSettings::value(calibrationPath(type));

	if (!ret.isNull())
		return ret.toDouble();

	switch (type) {
	case Model::Tlrns_value:
		return 0.0001;
	case Model::Pat_Ht_value:
		return 175.0;
	case Model::Pat_Wt_value:
		return 75.0;
	case Model::Lung_Ht_value:
		return 20;
	case Model::MV_value:
		return 0.1;
	case Model::CL_value:
		return 0.002617*175 - 0.1410;
	case Model::CO_value:
		return 6.45;
	case Model::LAP_value:
		return 5.0;
	case Model::Pal_value:
		return 0.0;
	case Model::Ppl_value:
		return -5.0;

	case Model::Kra:
		return 0.0601624004060655;
	case Model::Krv:
		return 0.0569240525006599;
	case Model::Krc:
		return 3135.049210184;

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
	 */
	int n = nElements();
	for (int i=0; i<n; i++) {
		Vessel &art = arteries[i];
		art.perivascular_press_a = 15.0 - 0.2*art.Ptp;
		art.perivascular_press_b = -16.0 - 0.2*art.Ptp;
		art.perivascular_press_c = -0.013 - 0.0002*art.Ptp;

		Vessel &vein = veins[i];
		vein.perivascular_press_a = 12.0 - 0.1*vein.Ptp;
		vein.perivascular_press_b = -12.0 - 0.2*vein.Ptp;
		vein.perivascular_press_c = -0.020 - 0.0002*vein.Ptp;
	}
}

void Model::getKz()
{
	/* Effect of lung volume on vessel dimensions */
	int n = nElements();
	// double Ptp = Pal - Ppl;

	for (int i=0; i<n; ++i) {
		arteries[i].length_factor =
		        fmax(1.0, exp((log(1-(1-MV)*exp(-CL*arteries[i].Ptp)))/3)/0.7368);

		veins[i].length_factor =
		        fmax(1.0, exp((log(1-(1-MV)*exp(-CL*veins[i].Ptp)))/3)/0.7368);
	}

	for( int i=0; i<n; i++ ){
		if (isOutsideLung(i)) {
			/* Vessels outside the lung have fixed length */
			arteries[i].Kz = arteries[i].R *
			                sqr(arteries[i].a + arteries[i].b * 35 ) / nSums;

			veins[i].Kz = veins[i].R /
			                (nSums*sqr(1+(veins[i].b*exp(veins[i].c*35))));
		}
		else {
			arteries[i].Kz = arteries[i].length_factor*arteries[i].R*
			                sqr(arteries[i].a+arteries[i].b*35) / nSums;

			veins[i].Kz = veins[i].length_factor*veins[i].R/
			                (nSums*sqr(1+(veins[i].b*exp(veins[i].c*35))));
		}
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
	arteries[0].pressure = PAP - arteries[0].flow * arteries[0].R;
	veins[0].pressure = LAP + veins[0].flow * veins[0].R;

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
	double R_tot = arteries[i].R +
	        1/(1/totalResistance( connection_first ) +
	           1/totalResistance( connection_first+1 )) +
	        veins[i].R;

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

	// Calculate flows in children based on pressure of parent
	for( int con=connection_first; con<=connection_first+1; con++ )
		veins[ con ].flow = arteries[ con ].flow =
						  ( arteries[i].pressure - veins[i].pressure ) /
						  ( arteries[con].total_R );

	// Calculate pressure in children vessels based on the calculated flow and their resistances
	for( int con=connection_first; con<=connection_first+1; con++ ){
		arteries[ con ].pressure = arteries[i].pressure - arteries[ con ].flow * arteries[ con ].R;
		veins[ con ].pressure = veins[i].pressure + veins[ con ].flow * veins[ con ].R;
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

	const double Ri = caps[i].R;
	const double Pin = 1.36 * con_artery.pressure - con_artery.GP; // convert pressures from mmHg => cmH20
	const double Pout = 1.36 * con_vein.pressure - con_vein.GP;

	const double x = Pout - Pal;
	const double y = Pin - Pal;
	const double FACC = y-x;
	const double FACC1 = 25.0-x;
	Capillary & cap = caps[i];
	const double Rz = getKrc() * nElements(n_generations) / nElements(16);

	if( x < 0 ){
		if( y < 0 )
			cap.R = FACC / con_artery.flow;
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

	cap.R = K1 * cap.R + K2 * Ri;
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
		max_deviation = qMax(i.result(), max_deviation);

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
	const int num_veins = numVeins();
	for (int i=0; i<num_veins; ++i) {
		if (isOutsideLung(i)) {
			// correct for vessels outside the lung
			veins[i].b = 10.0;
			veins[i].c = -10.0;
		}
		else {
			veins[i].b = 2.4307;
			veins[i].c = -0.2355;
		}
		veins[i].a = 0;
		veins[i].tone = 0;

		veins[i].CL = CL;
		veins[i].MV = MV;
	}

	const int num_arteries = numArteries();
	for (int i=0; i<num_arteries; ++i) {
		arteries[i].a = 0.2419;
		arteries[i].b = 0.02166;
		arteries[i].c = 0;
		arteries[i].tone = 0;

		arteries[i].CL = CL;
		arteries[i].MV = MV;
	}

	// Initialize capillaries
	const int nCapillaries = numCapillaries();
	for (int i=0; i<nCapillaries; i++) {
		caps[i].Alpha = 0.219;
		caps[i].Ho = 4.28;
		caps[i].R = getKrc() * nElements(n_generations) / nElements(16);
	}
}

void Model::initVesselBaselineResistances()
{
	double new_CL = 0.002617 * PatHt - 0.1410;
	CO = CI * BSA(PatHt, PatWt);

	setNewVesselCL(CL, new_CL);
	CL = new_CL;

	// corrected Kra and Krv
	double cKra = getKra()*(BSAz() / BSA(PatHt, PatWt));
	double cKrv = getKrv()*(BSAz() / BSA(PatHt, PatWt));

	initVesselBaselineResistances(cKra, cKrv, 1);
}

void Model::initVesselBaselineResistances(double cKra, double cKrv, int gen)
{
	// Get starting index and number of elements in the generation
	int start_index = startIndex( gen );
	int num_elements = nElements( gen );
	int n = num_elements + start_index;

	// Initialize elements in Arteries and Veins
	double a_factor = arteryResistanceFactor(gen);
	double v_factor = veinResistanceFactor(gen);
	for( int i=start_index; i<n; i++ ){
		int vessel_no = i - start_index;

		arteries[i].R = cKra * a_factor;
		veins[i].R = cKrv * v_factor;

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

		veins[i].GPz = arteries[i].GPz =
		                (vessel_no*exp((effective_ngen-gp_gen+1)*M_LN2)+
		                 exp((effective_ngen-gp_gen)*M_LN2)-1) /
		                (exp((effective_ngen)*M_LN2) - 2);
	}

	// Initialize more generations, if they exist
	if( gen < nGenerations())
		initVesselBaselineResistances(cKra, cKrv, gen+1); // R(n+1) = 0.5 * R(n)
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

		art.Ppl = Ppl - 0.55*art.GP;
		art.Ptp = Pal - art.Ppl;
		vein.Ppl = Ppl - 0.55*vein.GP;
		vein.Ptp = Pal - vein.Ppl;

		if (art.Ptp < 0)
			art.Ptp = 0;
		if (vein.Ptp < 0)
			vein.Ptp = 0;
	}
}

void Model::setNewVesselCL(double old_CL, double new_CL)
{
	/* Only override CL values that are not overrides from model-wide CL */
	const int n_arteries = numArteries();
	const int n_veins = numVeins();

	for (int i=0; i<n_arteries; ++i)
		if (!significantChange(old_CL, arteries[i].CL))
			arteries[i].CL = new_CL;

	for (int i=0; i<n_veins; ++i)
		if (!significantChange(old_CL, veins[i].CL))
			veins[i].CL = new_CL;
}

void Model::setNewVesselMV(double old_MV, double new_MV)
{
	/* Only override MV values that are not overrides from model-wide MV */
	const int n_arteries = numArteries();
	const int n_veins = numVeins();

	for (int i=0; i<n_arteries; ++i)
		if (!significantChange(old_MV, arteries[i].MV))
			arteries[i].MV = new_MV;

	for (int i=0; i<n_veins; ++i)
		if (!significantChange(old_MV, veins[i].MV))
			veins[i].MV = new_MV;
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
	SET_VALUE(MV);
	SET_VALUE(CL);
	SET_VALUE(Pal);
	SET_VALUE(Ppl);
	SET_VALUE(CO);
	SET_VALUE(CI);
	SET_VALUE(LAP);
	SET_VALUE(PatWt);
	SET_VALUE(PatHt);

	SET_VALUE(Kra_factor);
	SET_VALUE(Krv_factor);
	SET_VALUE(Krc_factor);

	SET_VALUE(art_calib.branch_ratio);
	SET_VALUE(art_calib.diam_ratio);
	SET_VALUE(art_calib.len_ratio);
	SET_VALUE(vein_calib.branch_ratio);
	SET_VALUE(vein_calib.diam_ratio);
	SET_VALUE(vein_calib.len_ratio);

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
			const Vessel &v = (type==1 ? arteries[n] : veins[n]);

			SET_VALUE(v.R);
			SET_VALUE(v.a);
			SET_VALUE(v.b);
			SET_VALUE(v.c);
			SET_VALUE(v.tone);

			SET_VALUE(v.GP);
			SET_VALUE(v.GPz);
			SET_VALUE(v.Ppl);
			SET_VALUE(v.Ptp);
			SET_VALUE(v.CL);
			SET_VALUE(v.MV);

			SET_VALUE(v.perivascular_press_a);
			SET_VALUE(v.perivascular_press_b);
			SET_VALUE(v.perivascular_press_c);

			SET_VALUE(v.length_factor);
			SET_VALUE(v.Kz);

			SET_VALUE(v.total_R);
			SET_VALUE(v.pressure);
			SET_VALUE(v.flow);

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
		const Capillary &cap = caps[n];

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
	SET_VALUE(MV);
	SET_VALUE(CL);
	SET_VALUE(Pal);
	SET_VALUE(Ppl);
	SET_VALUE(CO);
	SET_VALUE(CI);
	SET_VALUE(LAP);
	SET_VALUE(PatWt);
	SET_VALUE(PatHt);

	SET_VALUE(Kra_factor);
	SET_VALUE(Krv_factor);
	SET_VALUE(Krc_factor);

	SET_VALUE(art_calib.branch_ratio);
	SET_VALUE(art_calib.diam_ratio);
	SET_VALUE(art_calib.len_ratio);
	SET_VALUE(vein_calib.branch_ratio);
	SET_VALUE(vein_calib.diam_ratio);
	SET_VALUE(vein_calib.len_ratio);

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
		*this = Model(trans_pos, model_type, new_gen);
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
	GET_VALUE(MV);
	GET_VALUE(CL);
	GET_VALUE(Pal);
	GET_VALUE(Ppl);
	GET_VALUE(CO);
	GET_VALUE(CI);
	GET_VALUE(LAP);
	GET_VALUE(PatWt);
	GET_VALUE(PatHt);

	GET_VALUE(Kra_factor);
	GET_VALUE(Krv_factor);
	GET_VALUE(Krc_factor);

	GET_VALUE(art_calib.branch_ratio);
	GET_VALUE(art_calib.diam_ratio);
	GET_VALUE(art_calib.len_ratio);
	GET_VALUE(vein_calib.branch_ratio);
	GET_VALUE(vein_calib.diam_ratio);
	GET_VALUE(vein_calib.len_ratio);


	// force recalculation of Kr factors
	setKrFactors(Kra_factor, Krv_factor, Krc_factor);

	// load values of each vessel
	values.clear();

	if (progress)
		progress->setValue(progress->value()+1);

	double div_per_vessel = 998.0 / (nElements()*2+nElements(n_generations));
	double progress_value=0, current_value=0;

	q.prepare("SELECT value FROM vessel_values WHERE type=? AND vessel_idx=? AND key=? AND offset=?");
	int n_elements = nElements();
	for (int type=1; type<=2; ++type) {
		for (int n=0; n<n_elements; ++n) {
			Vessel &v = (type==1 ? arteries[n] : veins[n]);

			SET_VALUE(v.R);
			SET_VALUE(v.a);
			SET_VALUE(v.b);
			SET_VALUE(v.c);
			SET_VALUE(v.tone);

			SET_VALUE(v.GP);
			SET_VALUE(v.GPz);
			SET_VALUE(v.Ppl);
			SET_VALUE(v.Ptp);
			SET_VALUE(v.CL);
			SET_VALUE(v.MV);

			SET_VALUE(v.perivascular_press_a);
			SET_VALUE(v.perivascular_press_b);
			SET_VALUE(v.perivascular_press_c);

			SET_VALUE(v.length_factor);
			SET_VALUE(v.Kz);

			SET_VALUE(v.total_R);
			SET_VALUE(v.pressure);
			SET_VALUE(v.flow);

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
			GET_VALUE(v.a);
			GET_VALUE(v.b);
			GET_VALUE(v.c);
			GET_VALUE(v.tone);

			GET_VALUE(v.GP);
			GET_VALUE(v.GPz);
			GET_VALUE(v.Ppl);
			GET_VALUE(v.Ptp);
			GET_VALUE(v.CL);
			GET_VALUE(v.MV);

			GET_VALUE(v.perivascular_press_a);
			GET_VALUE(v.perivascular_press_b);
			GET_VALUE(v.perivascular_press_c);

			GET_VALUE(v.length_factor);
			GET_VALUE(v.Kz);

			GET_VALUE(v.total_R);
			GET_VALUE(v.pressure);
			GET_VALUE(v.flow);

			progress_value += div_per_vessel;
			while (progress && progress_value - current_value > 1.0) {
				progress->setValue(progress->value()+1);
				current_value += 1.0;
				qApp->processEvents();
			}
		}
	}

	n_elements = nElements(n_generations);
	values.clear();

	for (int n=0; n<n_elements; ++n) {
		Capillary &cap = caps[n];

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

