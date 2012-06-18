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
#include "cpuhelper.h"


extern const double K1;
extern const double K2;
extern const int nSums;

static inline double sqr(double n)
{
	return n*n;
}

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

	return max_deviation;
}

double CpuIntegrationHelper::integrateArtery(int vessel_index)
{
	Vessel & art = arteries()[vessel_index];

	const double hct = Hct();
	double Rtot = 0.0;
	double Pin = 0.0;
	double Rin = art.R;
	double Pout = art.pressure;

	double Ptm_i = 1.36 * ( Pout - art.tone ) - art.GP;
	double Ptm = Ptm_i;
	double Rs;
	double D_integral = 0.0;

	/* First segment is slightly different from others, so we pull it out */
	/* First 1/5th of generations is outside the lung - use different equation */

	const bool is_outside_lung = isOutsideLung(vessel_index);
	if (is_outside_lung)
		Ptm = Ptm - art.Ppl;
	else
		Ptm = Ptm - art.Ppl - art.perivascular_press_a -
		                art.perivascular_press_b * exp( art.perivascular_press_c * ( Ptm - art.Ppl ));

	const double dL = art.length / nSums;

	// min diameter is always first segment
	art.viscosity_factor = 0.0;
	art.volume = 0.0;

	if( Ptm < 0 ) {
		art.Dmin = 0.0;
		Rs = -Ptm/( 1.36 * art.flow ); // Starling Resistor
	}
	else {
		art.Dmin = art.D * sqrt(art.a + art.b*Ptm);
		Rs = art.Kz / sqr( art.a + art.b * Ptm );
		const double vf = viscosityFactor(art.Dmin, hct);
		Rs *= vf;
		art.viscosity_factor += vf;
		art.volume += M_PI * sqr(art.Dmin/2/1e4) * dL;
	}

	D_integral = art.Dmin;
	Pin = Pout + art.flow * Rs;
	Rtot += Rs;
	Pout = Pin;

	double D;
	for( int j=1; j<nSums; j++ ){

		Ptm = 1.36 * ( Pout - art.tone ) - art.GP;

		if (is_outside_lung)
			Ptm = Ptm - art.Ppl;
		else
			Ptm = Ptm - art.Ppl - art.perivascular_press_a -
			                art.perivascular_press_b * exp( art.perivascular_press_c * ( Ptm - art.Ppl ));

		const double A = art.a + art.b * Ptm;
		Rs = art.Kz / sqr(A);

		D = art.D * sqrt(A);
		D_integral += D;
		const double vf = viscosityFactor(D, hct);
		Rs *= vf;
		art.viscosity_factor += vf;
		art.volume += M_PI * sqr(D/2/1e4) * dL;

		Pin = Pout + art.flow * Rs;
		Rtot += Rs;
		Pout = Pin;
	}
	art.viscosity_factor /= nSums;
	art.Dmax = D; // max diameter is always last segment
	art.D_calc = D_integral /= nSums;

	art.R = Rtot;
	art.R = K1 * art.R + K2 * Rin;

	return fabs(Rin-art.R)/Rin;
}

double CpuIntegrationHelper::integrateVein(int vessel_index)
{
	Vessel & vein = veins()[vessel_index];

	const double hct = Hct();
	double Rtot = 0.0;
	double Pin = 0.0;
	double Rin = vein.R;
	double Pout = ((vessel_index == 0) ? LAP() : veins()[(vessel_index-1)/2].pressure);

	double Ptm = 1.36 * ( Pout - vein.tone ) - vein.GP;
	double Rs;
	double D_integral = 0.0;

	/* First segment is slightly different from others, so we pull it out */
	/* First 1/5th of generations is outside the lung - use different equation */
	const bool is_outside_lung = isOutsideLung(vessel_index);
	if (is_outside_lung)
		Ptm = Ptm - vein.Ppl;
	else
		Ptm = Ptm - vein.Ppl - vein.perivascular_press_a -
		                vein.perivascular_press_b * exp( vein.perivascular_press_c * ( Ptm - vein.Ppl ));

	const double dL =  vein.length / nSums;

	// min diameter is always first segment
	vein.viscosity_factor = 0.0;
	if( Ptm < 0 ) {
		vein.Dmin = 0.0;
		Rs = -Ptm/( 1.36 * vein.flow ); // Starling Resistor
	}
	else {
		vein.Dmin = vein.D / sqrt(1+vein.b * exp(vein.c*Ptm));
		Rs = vein.Kz * sqr(1.0 + vein.b * exp( vein.c * Ptm ));
		const double vf = viscosityFactor(vein.Dmin, hct);
		Rs *= vf;
		vein.viscosity_factor += vf;
		vein.volume += M_PI * sqr(vein.Dmin/2/1e4) * dL;
	}

	D_integral = vein.Dmin;
	Pin = Pout + vein.flow * Rs;
	Rtot = Rs;
	Pout = Pin;

	double D;
	for( int j=1; j<nSums; j++ ){
		Ptm = 1.36 * ( Pout - vein.tone ) - vein.GP;

		if (is_outside_lung)
			Ptm = Ptm - vein.Ppl;
		else
			Ptm = Ptm - vein.Ppl - vein.perivascular_press_a -
			                vein.perivascular_press_b * exp( vein.perivascular_press_c * ( Ptm - vein.Ppl ));

		const double A = 1.0 + vein.b * exp( vein.c * Ptm );
		Rs = vein.Kz * sqr(A);

		D = vein.D / sqrt(A);
		D_integral += D;
		const double vf = viscosityFactor(D, hct);
		Rs *= vf;
		vein.viscosity_factor += vf;
		vein.volume += M_PI * sqr(D/2/1e4) * dL;

		Pin = Pout + vein.flow * Rs;
		Rtot += Rs;
		Pout = Pin;
	}
	vein.viscosity_factor /= nSums;
	vein.Dmax = D; // max diameter is always last segment
	vein.D_calc = D_integral / nSums;

	vein.R = Rtot;
	vein.R = K1 * vein.R + K2 * Rin;

	return fabs(Rin-vein.R)/Rin;
}
