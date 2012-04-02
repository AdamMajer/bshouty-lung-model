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

	double Rtot = 0.0;
	double Pin = 0.0;
	double Rin = art.R;
	double Pout = art.pressure;

	double Ptm_i = 1.36 * ( Pout - art.tone ) - art.GP;
	double Ptm = Ptm_i;
	double Rs;

	/* First segment is slightly different from others, so we pull it out */
	/* First 1/5th of generations is outside the lung - use different equation */

	const bool is_outside_lung = isOutsideLung(vessel_index);
	if (is_outside_lung) //
		Ptm = Ptm - art.Ppl;
	else
		Ptm = Ptm - art.Ppl - art.perivascular_press_a -
		                art.perivascular_press_b * exp( art.perivascular_press_c * ( Ptm - art.Ppl ));

	if( Ptm < 0 )
		Rs = -Ptm/( 1.36 * art.flow ); // Starling Resistor
	else
		Rs = art.Kz / sqr( art.a + art.b * Ptm );

	Pin = Pout + art.flow * Rs;
	Rtot += Rs;
	Pout = Pin;

	for( int j=1; j<nSums; j++ ){
		Ptm = 1.36 * ( Pout - art.tone ) - art.GP;

		if (is_outside_lung)
			Ptm = Ptm - art.Ppl;
		else
			Ptm = Ptm - art.Ppl - art.perivascular_press_a -
			                art.perivascular_press_b * exp( art.perivascular_press_c * ( Ptm - art.Ppl ));

		Rs = art.Kz / sqr(art.a + art.b * Ptm);

		Pin = Pout + art.flow * Rs;
		Rtot += Rs;
		Pout = Pin;
	}
	art.R = Rtot;
	art.R = K1 * art.R + K2 * Rin;

	return fabs(Rin-art.R)/Rin;
}

double CpuIntegrationHelper::integrateVein(int vessel_index)
{
	Vessel & vein = veins()[vessel_index];

	double Rtot = 0.0;
	double Pin = 0.0;
	double Rin = vein.R;
	double Pout = ((vessel_index == 0) ? LAP() : veins()[(vessel_index-1)/2].pressure);

	double Ptm = 1.36 * ( Pout - vein.tone ) - vein.GP;
	double Rs;

	/* First segment is slightly different from others, so we pull it out */
	/* First 1/5th of generations is outside the lung - use different equation */
	const bool is_outside_lung = isOutsideLung(vessel_index);
	if (is_outside_lung)
		Ptm = Ptm - vein.Ppl;
	else
		Ptm = Ptm - vein.Ppl - vein.perivascular_press_a -
		                vein.perivascular_press_b * exp( vein.perivascular_press_c * ( Ptm - vein.Ppl ));

	if( Ptm < 0 )
		Rs = -Ptm/( 1.36 * vein.flow ); // Starling Resistor
	else {
		Rs = vein.Kz * sqr(1.0 + vein.b * exp( vein.c * Ptm ));
	}


	Pin = Pout + vein.flow * Rs;
	Rtot = Rs;
	Pout = Pin;

	for( int j=1; j<nSums; j++ ){
		Ptm = 1.36 * ( Pout - vein.tone ) - vein.GP;

		if (is_outside_lung)
			Ptm = Ptm - vein.Ppl;
		else
			Ptm = Ptm - vein.Ppl - vein.perivascular_press_a -
			                vein.perivascular_press_b * exp( vein.perivascular_press_c * ( Ptm - vein.Ppl ));

		Rs = vein.Kz * sqr(1.0 + vein.b * exp( vein.c * Ptm ));

		Pin = Pout + vein.flow * Rs;
		Rtot += Rs;
		Pout = Pin;
	}

	vein.R = Rtot;
	vein.R = K1 * vein.R + K2 * Rin;

	return fabs(Rin-vein.R)/Rin;
}
