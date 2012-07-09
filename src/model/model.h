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

#ifndef MODEL_H
#define MODEL_H

#include "disease.h"
#include <QPair>

/* Defined in model.cpp, used by integration helper. OpenCL code assuses
 * these values too
 */
extern const double K1;
extern const double K2;
extern const int nSums; // number of divisions in the integral

struct CalibrationFactors {
	double len_ratio;
	double diam_ratio;
	double branch_ratio;

	double gen_r[16];

	enum CalibrationType { Artery, Vein };

	// loads from saved calibration factors
	CalibrationFactors(CalibrationType type);

	// sets calibration factors
	CalibrationFactors(double l, double d, double b)
	        : len_ratio(l), diam_ratio(d), branch_ratio(b) {

		memset(gen_r, 0, sizeof(double)*16);
	}
};

struct Vessel
{
	double R; // mmHg*min/l
	double D; // um
	double D_calc, Dmin, Dmax; // um
	double volume; // cm**3
	double length; // um
	double length_factor; // adjustment to vessel Ptp
	double viscosity_factor; // in cP (mPa*s)

	double a,b,c;
	double tone;

	double GP; // gravity factor corrected for transducer position
	double GPz; // gravity factor, without correcting for transducer position
	double Ppl, Ptp;

	// calclated in GetParameters()
	double perivascular_press_a, perivascular_press_b, perivascular_press_c;

	double total_R; // mmHg*min/l
	double pressure; // mmHg
	double flow; // l/min

	double vessel_ratio; // n_model_vessels / real vessels

	// double cacheline_padding[1]; // padding to 64-byte caching boundary
};

struct Capillary
{
	double R;
	double Ho;
	double Alpha;

	char cacheline_padding[40];
};

extern bool operator==(const struct Vessel &a, const struct Vessel &b);
extern bool operator==(const struct Capillary &a, const struct Capillary &b);

class AbstractIntegrationHelper;
class QProgressDialog;
class QSqlDatabase;
class QString;

class Model {
	friend class AbstractIntegrationHelper;

public:
	/* DiseaseParam indicates that high-dword (16-bits of the integer)
	 * indicate Disease index in the disease list and its parameter number.
	 */
	enum DataType { Lung_Ht_value, Flow_value, LAP_value, Pal_value, Ppl_value,
	                Ptp_value, PAP_value, Rus_value, Rds_value, Rm_value,
	                Rt_value, PciA_value, PcoA_value, Tlrns_value,
	                Vrv_value, Vfrc_value, Vtlc_value,
	                Pat_Ht_value, Pat_Wt_value,
	                TotalR_value,
	                Krc,
	                Hct_value, PA_EVL_value, PA_Diam_value, PV_EVL_value, PV_Diam_value,
	                CO_value = Flow_value,
	                DiseaseParam = 0xFFFF
	};

	enum Transducer { Top, Middle, Bottom };

	enum ModelType { SingleLung=0, DoubleLung };

	Model( Transducer, ModelType, int n_generations );
	Model(const Model &other);
	virtual ~Model();

	virtual Model& operator=(const Model&);
	virtual Model* clone() const;

	// Number of generations in the model
	int nGenerations() const { return n_generations; }

	// number of elements on each side of the model
	int nElements() const { return (1 << nGenerations())-1; } // 2^nGenerations-1

	// Number of elements in a given generation of the model
	int nElements( unsigned n ) const { return 1 << (n-1); } // 2**(n-1)

	// Number of real vessels (using branching factor)
	int nVessels(CalibrationFactors::CalibrationType vessel_type, unsigned gen_no) const;

	static double measuredDiameterRatio(CalibrationFactors::CalibrationType vessel_type, unsigned gen_no);
	static double measuredLengthRatio(CalibrationFactors::CalibrationType vessel_type, unsigned gen_no);

	// starting index of first element in generation n
	int startIndex( int n ) const {
		return (1<<(n-1))-1; // 2**(n-1)-1
	}

	// generation number of the index
	int gen_no( int i ) const {
		int n=0;
		// increment is to account for 0-based indexing
		++i;
		while( i > 0 ){
			i>>=1;
			++n;
		}

		return n;
	}

	// First 1/5th of generations is deemed to be outside the lung, rounded up
	// so 15-gen model => 3 vessels outside
	//    16-gen model => 4 vessels outside
	bool isOutsideLung(int i) const {
		return gen_no(i) <= (nGenerations()+4)/5;
	}

	int nOutsideElements() const {
		return (1 << ((nGenerations()+4)/5))-1;
	}

	// Resistance factor from Generation (gen) to (gen+1)
	double arteryResistanceFactor(int gen);
	double veinResistanceFactor(int gen);
	double vesselResistanceFactor(int gen, const double *gen_r) const;

	double BSAz() const;
	static double BSA(double pat_ht, double pat_wt);

	int numArteries() const { return nElements(); }
	int numVeins() const { return nElements(); }
	int numCapillaries() const { return nElements(nGenerations()); }

	// get and set data
	const Vessel& artery( int gen, int index ) const;
	const Vessel& vein( int gen, int index ) const;
	const Capillary& capillary( int index ) const;

	void setArtery( int gen, int index, const Vessel & );
	void setVein( int gen, int index, const Vessel & );
	void setCapillary( int index, const Capillary & );

	virtual double getResult( DataType ) const;
	virtual bool setData( DataType, double );

	ModelType modelType() const { return model_type; }
	Transducer transducerPos() const;
	void setTransducerPos(Transducer trans);

	// does actual calculations
	virtual int calc( int max_iter = 100 ); // returns number of iterations

	bool calculationErrors() const;

	// load/save state to a database
	bool load(QSqlDatabase &db, int offset=0, QProgressDialog *progress=0);
	bool save(QSqlDatabase &db, int offset=0, QProgressDialog *progress=0);

	virtual bool isModified() const;

	void setAbort();
	bool isAbort() const { return abort_calculation; }

	int addDisease(const Disease &); // returns disease index
	void clearDiseases();
	const DiseaseList& diseases() const { return dis; }

	const Disease& disease(DataType hybrid_type) const;
	static int diseaseNo(DataType hybrid_type);
	static int diseaseParamNo(DataType hybrid_type);
	DataType diseaseHybridType(const Disease &disease, int param_no) const;
	static DataType diseaseHybridType(int disease_no, int param_no);

	// threadsafe
	virtual int progress() const { return prog; }

	/* Only used by the calibration functionality */
	void setKrFactors(double Krc);
	double getKrc();
	CalibrationFactors calibrationFactor(CalibrationFactors::CalibrationType) const;
	void setCalibrationRatios(const CalibrationFactors &art, const CalibrationFactors &vein);

	static QString calibrationPath(DataType type);
	static double calibrationValue(DataType);

protected:
	void getParameters();
	// void getKz();
	void vascPress();

	double totalResistance(int i);
	void calculateChildrenFlowPress(int i);

	double deltaCapillaryResistance(int i);
	bool deltaR();

	void initVesselBaselineCharacteristics();
	void initVesselBaselineResistances();
	void initVesselBaselineResistances(int gen);
	void calculateBaselineCharacteristics();

	double lengthFactor(const Vessel &v) const;

	virtual bool initDb(QSqlDatabase &db) const;
	virtual bool saveDb(QSqlDatabase &db, int offset, QProgressDialog *progress);
	virtual bool loadDb(QSqlDatabase &db, int offset, QProgressDialog *progress);

protected:
	DiseaseList dis;

private:
	double Tlrns, LungHt, Pal, Ppl, CO, CI, LAP;
	double Vrv, Vfrc, Vtlc;
	double PatWt, PatHt;
	double Hct;
	double PA_EVL, PA_diam, PV_EVL, PV_diam; // cm
	Transducer trans_pos;
	ModelType model_type;

	int n_generations;
	Vessel *arteries, *veins;
	Capillary *caps;

	bool modified_flag; // used by isModified() function
	int abort_calculation;
	bool model_reset;

	QAtomicInt prog; // progress is set 0-10000
	AbstractIntegrationHelper *integration_helper;


	/* Calibration constants */
	double Krc_factor;

	CalibrationFactors art_calib, vein_calib;
};

typedef QList<QPair<int, Model*> > ModelCalcList;

#endif

