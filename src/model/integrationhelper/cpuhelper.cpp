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

double CpuIntegrationHelper::rigidFlowVessel(Vessel &vein)
{
	/* NOTE: calc_dim is assumed empty, if supplied */
	const double hct = Hct();
	double Rin = vein.R;
	double Pin = vein.pressure_in;
	const double Pout = vein.pressure_out;

	// undefined pressure signals no flow (closed vessel(s) somewhere)
	if (isnan(Pout) || isnan(Pin))
		return 0.0;

	double Ptm = 1.35951002636 * ((Pin+Pout)/2.0 - vein.tone);
	double Rs;

	/* First segment is slightly different from others, so we pull it out */
	/* First 1/5th of generations is outside the lung - use different equation */

	// check if vessel is closed
	if (vein.D < 0.1) {
		vein.D_calc = 0.0;
		vein.D = 0.0;
		vein.Dmin = 0.0;
		vein.Dmax = 0.0;

		//			if (calc_dim)
		//				std::fill_n(calc_dim->begin(), nSums, 0.0);

		vein.viscosity_factor = std::numeric_limits<double>::infinity();
		vein.volume = 0;
		vein.R = std::numeric_limits<double>::infinity();
		return Rin > 1e100 ? 0 : 10.0;
	}

	Ptm = Ptm - vein.Ppl - vein.perivascular_press_a -
	      vein.perivascular_press_b * exp( vein.perivascular_press_c * ( Ptm - vein.Ppl ));

	const double dL =  vein.length;

	// min diameter is always first segment
	vein.viscosity_factor = 0.0;
	vein.volume = 0.0;
	double D = 0.0;
	if( Ptm < 0 ) {
		Rs = -Ptm/( 1.35951002636 * vein.flow ); // Starling Resistor
	}
	else {
		double new_Pin = Pin;
		double old_Pin;
		do {
			old_Pin = new_Pin;
			double avg_P = (new_Pin + Pout) / 2.0;
			Ptm = 1.35951002636 * ( avg_P - vein.tone );
			Ptm = Ptm - vein.Ppl - vein.perivascular_press_a -
			      vein.perivascular_press_b * exp( vein.perivascular_press_c * ( Ptm - vein.Ppl ));

			const double inv_A = (1.0 + vein.b * exp( vein.c * Ptm )) / 0.99936058722097668220 / vein.a;
			D = vein.D / sqrt(inv_A);
			const double vf = viscosityFactor(D, hct);
			Rs = 128*Kr/M_PI * vf * dL / sqr(sqr(D)) * vein.vessel_ratio;
			vein.viscosity_factor = vf;

			new_Pin = Pout + vein.flow * Rs;
			new_Pin = (new_Pin + old_Pin) / 2.0;
		} while (fabs(new_Pin - old_Pin)/old_Pin > Tlrns());
	}

	//		if (calc_dim) {
	//			calc_dim->push_back(vein.Dmin);
	//			calc_dim->push_back(vein.Dmin);
	//		}

	vein.volume = M_PI/4.0 * sqr(D) * dL;
	vein.Dmax = D; // max diameter is always last segment
	vein.Dmin = D;
	vein.D_calc = D;

	vein.volume = vein.volume / (1e9*vein.vessel_ratio); // um**3 => uL, and correct for real number of vessels
	vein.R = Rs;
	// vein.R = K1 * vein.R + K2 * Rin;
	vein.last_delta_R = fabs(Rin-vein.R)/Rin;

	return vein.last_delta_R;
}

double CpuIntegrationHelper::segmentedFlowVessel(Vessel &vein)
{
	return segmentedFlowVessel(vein, NULL);
}

double CpuIntegrationHelper::segmentedFlowVessel(Vessel &vein,
                                                 std::vector<double> *calc_dim)
{
	/* NOTE: calc_dim is assumed empty, if supplied */
	const double hct = Hct();
	double Rtot = 0.0;
	double Rin = vein.R;
	double P = vein.pressure_out; // pressure to the right (LAP) of the vessel

	// undefined pressure signals no flow (closed vessel(s) somewhere)
	if (isnan(P))
		return 0.0;

	double Ptm = 1.35951002636 * ( P - vein.tone );
	double Rs;
	double D_integral = 0.0;

	/* First segment is slightly different from others, so we pull it out */
	/* First 1/5th of generations is outside the lung - use different equation */

	// check if vessel is closed
	if (vein.D < 0.1) {
		vein.D_calc = vein.D = 0.0;
		vein.Dmax = vein.Dmin = vein.D;

		if (calc_dim)
			std::fill_n(calc_dim->begin(), nSums, 0.0);

		vein.viscosity_factor = std::numeric_limits<double>::infinity();
		vein.volume = 0;
		vein.R = std::numeric_limits<double>::infinity();
		return Rin > 1e100 ? 0 : 10.0;
	}

	Ptm = Ptm - vein.Ppl - vein.perivascular_press_a -
	      vein.perivascular_press_b * exp( vein.perivascular_press_c * ( Ptm - vein.Ppl ));

	const double dL =  vein.length / (double)nSums;

	// min diameter is always first segment
	vein.viscosity_factor = 0.0;
	vein.volume = 0.0;
	if( Ptm < 0 ) {
		vein.Dmin = 0.0;
		Rs = -Ptm/( 1.35951002636 * vein.flow ); // Starling Resistor
	}
	else {
		const double inv_A = (1.0 + vein.b * exp( vein.c * Ptm )) / 0.99936058722097668220 / vein.a;
		double area = vein.a / vein.max_a;
		double A = ((1/inv_A - 1.0) * area + 1.0)*area;
		vein.Dmin = vein.D * sqrt(A);

		//vein.Dmin = vein.D / sqrt(inv_A);
		const double vf = viscosityFactor(vein.Dmin, hct);
		Rs = 128*Kr/M_PI * vf * dL / sqr(sqr(vein.Dmin)) * vein.vessel_ratio;
		vein.viscosity_factor += vf;
		vein.volume += M_PI/4.0 * sqr(vein.Dmin) * dL;

		if (calc_dim)
			calc_dim->push_back(vein.Dmin);
	}

	D_integral = vein.Dmin;
	P = P + vein.flow * Rs;
	Rtot = Rs;

	double D=0.0;
	for( int j=1; j<nSums; j++ ) {
		Ptm = 1.35951002636 * ( P - vein.tone );

		Ptm = Ptm - vein.Ppl - vein.perivascular_press_a -
		      vein.perivascular_press_b * exp( vein.perivascular_press_c * ( Ptm - vein.Ppl ));

		// corrected to Ptp=0, Ptm=35 cmH2O
		const double inv_A = (1.0 + vein.b * exp( vein.c * Ptm )) / 0.99936058722097668220 / vein.a;
		const double area = vein.a / vein.max_a;
		const double A = ((1/inv_A - 1.0) * area + 1.0)*area;
		D = vein.D * sqrt(A);

		D_integral += D;
		const double vf = viscosityFactor(D, hct);

		if (calc_dim)
			calc_dim->push_back(D);

		Rs = 128*Kr/M_PI * vf * dL / sqr(sqr(D)) * vein.vessel_ratio;
		vein.viscosity_factor += vf;
		vein.volume += M_PI/4.0 * sqr(D) * dL;

		P = P + vein.flow * Rs;
		Rtot += Rs;
	}
	vein.viscosity_factor /= nSums;
	vein.Dmax = D; // max diameter is always last segment
	vein.D_calc = D_integral / nSums;

	vein.volume = vein.volume / (1e9*vein.vessel_ratio); // um**3 => uL, and correct for real number of vessels
	vein.R = Rtot;
	// vein.R = K1 * vein.R + K2 * Rin;
	vein.last_delta_R = fabs(Rin-vein.R)/Rin;

	return vein.last_delta_R;
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

	qDebug("max deviation from CPU integration: %f", max_deviation);
	return max_deviation;
}

double CpuIntegrationHelper::vesselIntegrationThread(double (CpuIntegrationHelper::*func)(Vessel &))
{
	double ret = 0.0;
	int n = nElements();
	int i;

	Vessel *v = arteries();
	while ((i=artery_no.fetchAndAddOrdered(1024)) < n) {
		int max_pos = std::min(n, i+1024);
		for (int j=i; j<max_pos; ++j)
			ret = std::max(ret, (this->*func)(v[j]));
	}

	v = veins();
	while ((i=vein_no.fetchAndAddOrdered(1024)) < n) {
		int max_pos = std::min(n, i+1024);
		for (int j=i; j<max_pos; ++j)
			ret = std::max(ret, (this->*func)(v[i]));
	}

	return ret;
}
