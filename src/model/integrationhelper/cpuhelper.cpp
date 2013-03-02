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

#include <QtConcurrentRun>
#include <QtConcurrentMap>
#include <QFuture>
#include <QFutureSynchronizer>
#include <QThread>
#include "common.h"
#include <limits>
#include "cpuhelper.h"
#include <vector>

extern const double K1;
extern const double K2;
extern const int nSums;

const double Kr = 1.2501e8; // cP/um**3 => mmHg*min/l

static inline double viscosityFactor(double D, double Hct)
{
	const double C = (0.8+exp(-0.075*D)) * ((1/(1 + 1e-11*pow(D, 12)))-1.0) + (1/(1+1e-11*pow(D,12)));
	const double Mi45 = 220 * exp(-1.3*D) - 2.44*exp(-0.06*pow(D, 0.645)) + 3.2;
	return (1.0 + (Mi45-1.0)*(pow(1-Hct, C)-1)/(pow(1.0-0.45, C)-1)) / 3.2;
}

CpuIntegrationHelper::CpuIntegrationHelper(Model *model, Model::IntegralType type)
        : AbstractIntegrationHelper(model, type)
{

}

double CpuIntegrationHelper::segmentedVessels()
{
	return vesselIntegration(&CpuIntegrationHelper::segmentedFlowVessel);
}

double CpuIntegrationHelper::rigidVessels()
{
	return vesselIntegration(&CpuIntegrationHelper::rigidFlowVessel);
}

void CpuIntegrationHelper::integrateWithDimentions(Vessel::Type t,
                                                   int gen,
                                                   int idx,
                                                   std::vector<double> &calc_dim)
{
	calc_dim.clear();
	calc_dim.reserve(nSums);
	Vessel *v_ptr = (t==Vessel::Artery) ? arteries() : veins();
	segmentedFlowVessel(v_ptr[index(gen, idx)], &calc_dim);
}

double CpuIntegrationHelper::capillaryResistances()
{
	QFutureSynchronizer<double> threads;
	int thread_count = QThread::idealThreadCount();
	if (thread_count < 1)
		thread_count = 4;

	cap_no = 0;

	while (thread_count--)
		threads.addFuture(
		        QtConcurrent::run(this,
		                &CpuIntegrationHelper::capillaryThread));

	threads.waitForFinished();

	double max_deviation = 0;
	foreach (QFuture<double> future, threads.futures())
		max_deviation = qMax(max_deviation, future.result());

	return max_deviation;
}

double CpuIntegrationHelper::capillaryResistance(Capillary &cap)
{
	const double Ri = cap.R;
	const double & Ptmv = cap.pressure_out;

	switch (cap.open_state) {
	case Capillary_Closed:
		cap.last_delta_R = 0.0;

		if (isinf(cap.R))
			return 0.0;

		cap.R = std::numeric_limits<double>::infinity();
		return 1.0;

	case Capillary_Auto:
		break;
	}


	/* Calculate pressure in as a function of constant flow and pressure out.
	 * Based on this, calculate R
	 */
	double Ptma;

	if (isnan(cap.flow) || isnan(Ptmv) || cap.flow < 1e-50) {
		// flow is 0, Ptma == Ptmv, R is unchanged
		Ptma = Ptmv;
		cap.last_delta_R = 0.0;
		return 0.0;
	}
	else if (Ptmv >= 25.0) {
		// entire flow in max opened capillaries
		Ptma = Ptmv + cap.flow*cap.Krc/cap.F3;
	}
	else {
		const double Fout = sqr(sqr(1+cap.Alpha*std::max(0.0, Ptmv)));
		const double effective_flow = cap.flow / (1.0 - 0.1 + 0.1*exp(-std::min(-0.0, Ptmv)/(2*5.3*5.3)));
		Ptma = (sqrt(sqrt(4*cap.Alpha*effective_flow*cap.Krc + Fout)) - 1.0)/cap.Alpha;
		if (Ptma > 25.0) {
			// flow in region where part of the capillary is maximally open
			// and part of it is less than maximally open.
			Ptma = (effective_flow*cap.Krc - (cap.F4 - Fout)/(4*cap.Alpha))/cap.F3 + 25.0;
		}
	}

	if (Ptma < Ptmv)
		throw "error";

	// calculate effective resistance based on the pressures
	cap.R = (Ptma - Ptmv) / cap.flow;

	if (Ri > 1e10 && cap.R > Ri && Ptmv < 0) {
		// capillary is effectively closed
		cap.R = std::numeric_limits<double>::infinity();
		cap.last_delta_R = 1.0;
		return 1.0;
	}

	cap.last_delta_R = fabs(cap.R-Ri)/Ri;
	return cap.last_delta_R; // return different from target tolerance
}

double CpuIntegrationHelper::rigidFlowVessel(Vessel &v)
{
	/* NOTE: calc_dim is assumed empty, if supplied */
	const double hct = Hct();
	double Rin = v.R;
	double Pin = v.pressure_in;
	const double Pout = v.pressure_out;

	// undefined pressure signals no flow (closed vessel(s) somewhere)
	if (isnan(Pout) || isnan(Pin)) {
		v.D_calc = 0.0;
		v.Dmin = 0.0;
		v.Dmax = 0.0;
		return 0.0;
	}

	double Ptm = 1.35951002636 * ((Pin+Pout)/2.0 - v.tone);
	double Rs;

	/* First segment is slightly different from others, so we pull it out */
	/* First 1/5th of generations is outside the lung - use different equation */

	// check if vessel is closed
	if (v.D < 0.1) {
		v.D_calc = 0.0;
		v.D = 0.0;
		v.Dmin = 0.0;
		v.Dmax = 0.0;

		//			if (calc_dim)
		//				std::fill_n(calc_dim->begin(), nSums, 0.0);

		v.viscosity_factor = std::numeric_limits<double>::infinity();
		v.volume = 0;
		v.R = std::numeric_limits<double>::infinity();
		return Rin > 1e100 ? 0 : 10.0;
	}

	Ptm = Ptm - v.Ppl - v.perivascular_press_a -
	      v.perivascular_press_b * exp( v.perivascular_press_c * ( Ptm - v.Ppl ));

	const double dL =  v.length;

	// min diameter is always first segment
	v.viscosity_factor = 0.0;
	v.volume = 0.0;
	double D = 0.0;
	if( Ptm < 0 ) {
		Rs = -Ptm/( 1.35951002636 * v.flow ); // Starling Resistor
	}
	else {
		double new_Pin = Pin;
		double old_Pin;
		do {
			old_Pin = new_Pin;
			double avg_P = (new_Pin + Pout) / 2.0;
			Ptm = 1.35951002636 * ( avg_P - v.tone );
			Ptm = Ptm - v.Ppl - v.perivascular_press_a -
			      v.perivascular_press_b * exp( v.perivascular_press_c * ( Ptm - v.Ppl ));

			// const double inv_A = (1.0 + v.b * exp( v.c * Ptm )) / 0.99936058722097668220 / v.a;
			D = v.D*v.gamma - (v.gamma-1.0)*v.D*exp(-Ptm*v.phi/(v.gamma-1.0));
			const double vf = viscosityFactor(D, hct);
			Rs = 128*Kr/M_PI * vf * dL / sqr(sqr(D)) * v.vessel_ratio;
			v.viscosity_factor = vf;

			new_Pin = Pout + v.flow * Rs;
			new_Pin = (new_Pin + old_Pin) / 2.0;
		} while (fabs(new_Pin - old_Pin)/old_Pin > Tlrns());
	}

	//		if (calc_dim) {
	//			calc_dim->push_back(v.Dmin);
	//			calc_dim->push_back(v.Dmin);
	//		}

	v.volume = M_PI/4.0 * sqr(D) * dL;
	v.Dmax = D; // max diameter is always last segment
	v.Dmin = D;
	v.D_calc = D;

	v.volume = v.volume / (1e9*v.vessel_ratio); // um**3 => uL, and correct for real number of vessels
	v.R = Rs;
	// v.R = K1 * v.R + K2 * Rin;
	v.last_delta_R = fabs(Rin-v.R)/Rin;

	return v.last_delta_R;
}

double CpuIntegrationHelper::segmentedFlowVessel(Vessel &v)
{
	/* In case where flow is nil, the vessel has constant pressure
	 * across it and then the faster algorithm is really really the same
	 * and nSums times faster.
	 */
	if (v.flow == 0.0)
		return rigidFlowVessel(v);

	return segmentedFlowVessel(v, NULL);
}

double CpuIntegrationHelper::segmentedFlowVessel(Vessel &v,
                                                 std::vector<double> *calc_dim)
{
	/* NOTE: calc_dim is assumed empty, if supplied */
	const double hct = Hct();
	double Rtot = 0.0;
	double Rin = v.R;
	double P = v.pressure_out; // pressure to the right (LAP) of the vessel

	// undefined pressure signals no flow (closed vessel(s) somewhere)
	if (isnan(P))
		return 0.0;

	double Ptm = 1.35951002636 * ( P - v.tone );
	double Rs;
	double D_integral = 0.0;

	/* First segment is slightly different from others, so we pull it out */
	/* First 1/5th of generations is outside the lung - use different equation */

	// check if vessel is closed
	if (v.D < 0.1) {
		v.D_calc = v.D = 0.0;
		v.Dmax = v.Dmin = v.D;

		if (calc_dim)
			std::fill_n(calc_dim->begin(), nSums, 0.0);

		v.viscosity_factor = std::numeric_limits<double>::infinity();
		v.volume = 0;
		v.R = std::numeric_limits<double>::infinity();
		return Rin > 1e100 ? 0 : 10.0;
	}

	Ptm = Ptm - v.Ppl - v.perivascular_press_a -
	      v.perivascular_press_b * exp( v.perivascular_press_c * ( Ptm - v.Ppl ));

	const double dL =  v.length / (double)nSums;

	// min diameter is always first segment
	v.viscosity_factor = 0.0;
	v.volume = 0.0;
	if( Ptm < 0 ) {
		v.Dmin = 0.0;
		Rs = -Ptm/( 1.35951002636 * v.flow ); // Starling Resistor
	}
	else {
		/*
		const double inv_A = (1.0 + v.b * exp( v.c * Ptm )) / 0.99936058722097668220 / v.a;
		double area = v.a / v.max_a;
		double A = ((1/inv_A - 1.0) * area + 1.0)*area;
		v.Dmin = v.D * sqrt(A);
		*/

		v.Dmin = v.D*v.gamma - (v.gamma-1.0)*v.D*exp(-Ptm*v.phi/(v.gamma-1.0));

		//v.Dmin = v.D / sqrt(inv_A);
		const double vf = viscosityFactor(v.Dmin, hct);
		Rs = 128*Kr/M_PI * vf * dL / sqr(sqr(v.Dmin)) * v.vessel_ratio;
		v.viscosity_factor += vf;
		v.volume += M_PI/4.0 * sqr(v.Dmin) * dL;

		if (calc_dim)
			calc_dim->push_back(v.Dmin);
	}

	D_integral = v.Dmin;
	P = P + v.flow * Rs;
	Rtot = Rs;

	double D=0.0;
	for( int j=1; j<nSums; j++ ) {
		Ptm = 1.35951002636 * ( P - v.tone );

		Ptm = Ptm - v.Ppl - v.perivascular_press_a -
		      v.perivascular_press_b * exp( v.perivascular_press_c * ( Ptm - v.Ppl ));

		// corrected to Ptp=0, Ptm=35 cmH2O
		/*
		const double inv_A = (1.0 + v.b * exp( v.c * Ptm )) / 0.99936058722097668220 / v.a;
		const double area = v.a / v.max_a;
		const double A = ((1/inv_A - 1.0) * area + 1.0)*area;
		D = v.D * sqrt(A);
		*/

		D = v.D*v.gamma - (v.gamma-1.0)*v.D*exp(-Ptm*v.phi/(v.gamma-1.0));

		D_integral += D;
		const double vf = viscosityFactor(D, hct);

		if (calc_dim)
			calc_dim->push_back(D);

		Rs = 128*Kr/M_PI * vf * dL / sqr(sqr(D)) * v.vessel_ratio;
		v.viscosity_factor += vf;
		v.volume += M_PI/4.0 * sqr(D) * dL;

		P = P + v.flow * Rs;
		Rtot += Rs;
	}
	v.viscosity_factor /= nSums;
	v.Dmax = D; // max diameter is always last segment
	v.D_calc = D_integral / nSums;

	v.volume = v.volume / (1e9*v.vessel_ratio); // um**3 => uL, and correct for real number of vessels
	v.R = Rtot;
	// v.R = K1 * v.R + K2 * Rin;
	v.last_delta_R = fabs(Rin-v.R)/Rin;

	return v.last_delta_R;
}

double CpuIntegrationHelper::vesselIntegration(double(CpuIntegrationHelper::* func)(Vessel&))
{
	QFutureSynchronizer<double> threads;
	int thread_count = QThread::idealThreadCount();
	if (thread_count < 1)
		thread_count = 4;

	artery_no = 0;
	vein_no = 0;

	while (thread_count--)
		threads.addFuture(
		        QtConcurrent::run(this,
		                &CpuIntegrationHelper::vesselIntegrationThread,
		                func));

	threads.waitForFinished();

	double max_deviation = 0;
	foreach (QFuture<double> future, threads.futures())
		max_deviation = qMax(max_deviation, future.result());

	return max_deviation;
}

double CpuIntegrationHelper::vesselIntegrationThread(double (CpuIntegrationHelper::*func)(Vessel &))
{
	double ret = 0.0;
	int i;

	int n = nArteries();
	Vessel *v = arteries();
	while ((i=artery_no.fetchAndAddOrdered(1024)) < n) {
		int max_pos = std::min(n, i+1024);
		for (int j=i; j<max_pos; ++j)
			ret = std::max(ret, (this->*func)(v[j]));
	}

	n = nVeins();
	v = veins();
	while ((i=vein_no.fetchAndAddOrdered(1024)) < n) {
		int max_pos = std::min(n, i+1024);
		for (int j=i; j<max_pos; ++j)
			ret = std::max(ret, (this->*func)(v[j]));
	}

	return ret;
}

double CpuIntegrationHelper::capillaryThread()
{
	double ret = 0.0;
	int i;

	int n = nCaps();
	Capillary *c = capillaries();

	while ((i=cap_no.fetchAndAddOrdered(1024)) < n) {
		int max_pos = std::min(n, i+1024);
		for (int j=i; j<max_pos; ++j)
			ret = std::max(ret, capillaryResistance(c[j]));
	}
	return ret;
}

