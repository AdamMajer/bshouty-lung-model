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
#include <limits>
#include "cpuhelper.h"


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

CpuIntegrationHelper::CpuIntegrationHelper(Model *model)
        : AbstractIntegrationHelper(model)
{

}

double CpuIntegrationHelper::integrate()
{
	const int n_elements = nElements();
	std::vector<QFuture<double> > futures;
	std::vector<int> v(n_elements);

	// reserve space for all veins and arteries
	futures.reserve(n_elements * 2);

	for (int i=0; i<n_elements; ++i) {
		v[i] = i;

		futures.push_back(QtConcurrent::run(this, &CpuIntegrationHelper::integrateArtery, v[i]));
		futures.push_back(QtConcurrent::run(this, &CpuIntegrationHelper::integrateVein, v[i]));
	}

	double max_deviation = 0;
	for (int i=0; i<n_elements*2; ++i)
		max_deviation = qMax(max_deviation, futures[i].result());

	qDebug("max deviation from CPU integration: %f", max_deviation);

	return max_deviation;
}

double CpuIntegrationHelper::integrateArtery(int vessel_index)
{
	Vessel & art = arteries()[vessel_index];

	const double hct = Hct();
	double Rtot = 0.0;
	double Rin = art.R;
	double P = art.pressure;

	// undefined pressure signals no flow (closed vessel(s) somewhere)
	if (isnan(P))
		return 0.0;

	double Ptm_i = 1.35951002636 * ( P - art.tone ) - art.GP;
	double Ptm = Ptm_i;
	double Rs;
	double D_integral = 0.0;

	/* First segment is slightly different from others, so we pull it out */
	/* First 1/5th of generations is outside the lung - use different equation */

	// check if vessel is closed
	if (art.D < 0.1) {
		art.D_calc = art.D = 0.0;
		art.Dmax = art.Dmin = art.D;

		art.viscosity_factor = std::numeric_limits<double>::infinity();
		art.volume = 0;
		art.R = std::numeric_limits<double>::infinity();
		return Rin > 1e100 ? 0 : 10.0;
	}

	const bool is_outside_lung = isOutsideLung(vessel_index);
	if (is_outside_lung)
		Ptm = Ptm - art.Ppl;
	else
		Ptm = Ptm - art.Ppl - art.perivascular_press_a -
		                art.perivascular_press_b * exp( art.perivascular_press_c * ( Ptm - art.Ppl ));

	const double dL = art.length * art.length_factor / nSums;

	// min diameter is always first segment
	art.viscosity_factor = 0.0;
	art.volume = 0.0;

	if( Ptm < 0 ) {
		art.Dmin = 0.0;
		Rs = -Ptm/( 1.35951002636 * art.flow ); // Starling Resistor
	}
	else {
		const double A = art.a + art.b*Ptm;
		art.Dmin = art.D * sqrt(A);
		const double vf = viscosityFactor(art.Dmin, hct);

		Rs = 128*Kr/M_PI * vf * dL / sqr(sqr(art.Dmin)) * art.vessel_ratio;
		art.viscosity_factor += vf;
		art.volume += M_PI/4.0 * sqr(art.Dmin) * dL;
	}

	D_integral = art.Dmin;
	P = P + art.flow * Rs;
	Rtot += Rs;

	double D;
	for( int j=1; j<nSums; j++ ){

		Ptm = 1.35951002636 * ( P - art.tone ) - art.GP;

		if (is_outside_lung)
			Ptm = Ptm - art.Ppl;
		else
			Ptm = Ptm - art.Ppl - art.perivascular_press_a -
			                art.perivascular_press_b * exp( art.perivascular_press_c * ( Ptm - art.Ppl ));

		const double A = art.a + art.b * Ptm;
		D = art.D * sqrt(A);
		D_integral += D;
		const double vf = viscosityFactor(D, hct);

		Rs = 128*Kr/M_PI * vf * dL / sqr(sqr(D)) * art.vessel_ratio;
		art.viscosity_factor += vf;
		art.volume += M_PI/4.0 * sqr(D) * dL;

		P = P + art.flow * Rs;
		Rtot += Rs;
	}
	art.viscosity_factor /= nSums;
	art.Dmax = D; // max diameter is always last segment
	art.D_calc = D_integral / nSums;

	art.volume = art.volume / (1e9*art.vessel_ratio); // um**3 => uL, and correct for real number of vessels
	art.R = Rtot;
	// art.R = K1 * art.R + K2 * Rin;

	return fabs(Rin-art.R)/Rin;
}

double CpuIntegrationHelper::integrateVein(int vessel_index)
{
	Vessel & vein = veins()[vessel_index];

	const double hct = Hct();
	double Rtot = 0.0;
	double Rin = vein.R;
	double P = ((vessel_index == 0) ? LAP() : veins()[(vessel_index-1)/2].pressure);

	// undefined pressure signals no flow (closed vessel(s) somewhere)
	if (isnan(P))
		return 0.0;

	double Ptm = 1.35951002636 * ( P - vein.tone ) - vein.GP;
	double Rs;
	double D_integral = 0.0;

	/* First segment is slightly different from others, so we pull it out */
	/* First 1/5th of generations is outside the lung - use different equation */

	// check if vessel is closed
	if (vein.D < 0.1) {
		vein.D_calc = vein.D = 0.0;
		vein.Dmax = vein.Dmin = vein.D;

		vein.viscosity_factor = std::numeric_limits<double>::infinity();
		vein.volume = 0;
		vein.R = std::numeric_limits<double>::infinity();
		return Rin > 1e100 ? 0 : 10.0;
	}

	const bool is_outside_lung = isOutsideLung(vessel_index);
	if (is_outside_lung)
		Ptm = Ptm - vein.Ppl;
	else
		Ptm = Ptm - vein.Ppl - vein.perivascular_press_a -
		                vein.perivascular_press_b * exp( vein.perivascular_press_c * ( Ptm - vein.Ppl ));

	const double dL =  vein.length * vein.length_factor / (double)nSums;

	// min diameter is always first segment
	vein.viscosity_factor = 0.0;
	vein.volume = 0.0;
	if( Ptm < 0 ) {
		vein.Dmin = 0.0;
		Rs = -Ptm/( 1.35951002636 * vein.flow ); // Starling Resistor
	}
	else {
		const double inv_A = (1.0 + vein.b * exp( vein.c * Ptm )) / 0.99936058722097668220;
		vein.Dmin = vein.D / sqrt(inv_A);
		const double vf = viscosityFactor(vein.Dmin, hct);
		Rs = 128*Kr/M_PI * vf * dL / sqr(sqr(vein.Dmin)) * vein.vessel_ratio;
		vein.viscosity_factor += vf;
		vein.volume += M_PI/4.0 * sqr(vein.Dmin) * dL;
	}

	D_integral = vein.Dmin;
	P = P + vein.flow * Rs;
	Rtot = Rs;

	double D;
	for( int j=1; j<nSums; j++ ) {
		Ptm = 1.35951002636 * ( P - vein.tone ) - vein.GP;

		if (is_outside_lung)
			Ptm = Ptm - vein.Ppl;
		else
			Ptm = Ptm - vein.Ppl - vein.perivascular_press_a -
			                vein.perivascular_press_b * exp( vein.perivascular_press_c * ( Ptm - vein.Ppl ));

		// corrected to Ptp=0, Ptm=35 cmH2O
		const double inv_A = (1.0 + vein.b * exp( vein.c * Ptm )) / 0.99936058722097668220;
		D = vein.D / sqrt(inv_A);
		D_integral += D;
		const double vf = viscosityFactor(D, hct);

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

	return fabs(Rin-vein.R)/Rin;
}
