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

struct Vessel
{
	double R;
	double a,b,c;
	double tone;

	double GP; // gravity factor corrected for transducer position
	double GPz; // gravity factor, without correcting for transducer position
	double Ppl, Ptp;

	// calclated in GetParameters()
	double perivascular_press_a, perivascular_press_b, perivascular_press_c;

	// calculated in getKz()
	double length_factor;
	double Kz;

	double total_R;
	double pressure;
	double flow;

	double MV, CL;
};

struct Capillary
{
	double R;
	double Ho;
	double Alpha;
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
	                Rt_value, PciA_value, PcoA_value, Tlrns_value, MV_value, CL_value,
	                Pat_Ht_value, Pat_Wt_value,
	                TotalR_value,
	                CO_value = Flow_value,
	                DiseaseParam = 0xFFFF
	};

	enum Transducer { Top, Middle, Bottom };

	Model( Transducer, int n_generations );
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

	// First 1/5th of generations is deemed to be outside the lung
	bool isOutsideLung(int i) const {
		return gen_no(i) <= nGenerations()/5;
	}

	int nOutsideElements() const {
		return (1 << (nGenerations()/5))-1;
	}

	// Resistance factor from Generation (gen) to (gen+1)
	double arteryResistanceFactor(int gen) const;
	double veinResistanceFactor(int gen) const;
	double vesselResistanceFactor(int gen, const double *gen_r) const;

	static double BSAz() { return BSA(175, 75); }
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

protected:
	double getKra() const;
	double getKrv() const;
	double getKrc() const;

	void getParameters();
	void getKz();
	void vascPress();

	double totalResistance(int i);
	void calculateChildrenFlowPress(int i);

	double deltaCapillaryResistance(int i);
	bool deltaR();

	void initVesselBaselineCharacteristics();
	void initVesselBaselineResistances();
	void initVesselBaselineResistances(double cKra, double cKrv, int gen);
	void calculateBaselineCharacteristics();

	void setNewVesselCL(double old_CL, double new_CL);
	void setNewVesselMV(double old_MV, double new_MV);

	virtual bool initDb(QSqlDatabase &db) const;
	virtual bool saveDb(QSqlDatabase &db, int offset, QProgressDialog *progress);
	virtual bool loadDb(QSqlDatabase &db, int offset, QProgressDialog *progress);

protected:
	DiseaseList dis;

private:
	double Tlrns, LungHt, MV, CL, Pal, Ppl, CO, CI, LAP;
	double PatWt, PatHt;
	Transducer trans_pos;

	int n_generations;
	Vessel *arteries, *veins;
	Capillary *caps;

	bool modified_flag; // used by isModified() function
	int abort_calculation;
	bool model_reset;

	QAtomicInt prog; // progress is set 0-10000
	AbstractIntegrationHelper *integration_helper;
};

typedef QList<QPair<int, Model*> > ModelCalcList;

#endif

