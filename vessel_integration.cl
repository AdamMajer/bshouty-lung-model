#define WIDTH 32

struct Vessel {
	float R;
	float pressure;
	float GP;
	float tone;
	float flow;
	float Ppl;

	float a;
	float b;
	float c;

	float peri_a;
	float peri_b;
	float peri_c;

	float D;
	float len;

	float vessel_ratio;

	float pad[1]; // pad to 64 bytes
};

struct Result {
	float R;
	float D;
	float Dmin;
	float Dmax;
	float volume;
	float viscosity_factor;

	float pad[10]; // pad to 64 bytes
};

__constant float Kr = 1.2501e8; // cP/um**3 => mmHg*min/l
__constant int nSums = 16;
// #define Kr 1.2501e8

inline float sqr(float n)
{
	return n*n;
}

inline float viscosityFactor(float D, float Hct)
{
	const float C = (0.8+exp((float)-0.075*D)) * ((1/(1 + 1e-11*pown(D, 12)))-1.0) + (1/(1+1e-11*pown(D,12)));
	const float Mi45 = 220 * exp((float)-1.3*D) - 2.44*exp(-0.06*powr(D, (float)0.645)) + 3.2;
	return (1.0 + (Mi45-1.0)*(powr((float)1.0-Hct, C)-1.0)/(powr((float)1.0-(float)0.45, C)-1.0)) / 3.2;
}


__kernel void integrateInsideArtery(
                float hct,
                __global __read_only struct Vessel *v, 
                __global __write_only struct Result *result)
{
	const size_t vessel_index = get_global_id(0) + get_global_id(1)*WIDTH;
	const struct Vessel art = v[vessel_index];

	float Rtot = 0.0;
	float Pout = art.pressure;

	float Ptm = 1.35951002636 * ( Pout - art.tone ) - art.GP;
	float Rs;

	float D = 0.0;
	float D_integral = 0.0;
	float volume = 0.0;
	float viscosity = 0.0;
	float dL = art.len / nSums;

	Ptm = Ptm - art.Ppl - art.peri_a - art.peri_b * exp( art.peri_c * ( Ptm - art.Ppl ));
	if( Ptm < 0 ) {
		Rs = -Ptm/( 1.35951002636 * art.flow ); // Starling Resistor
	}
	else {
		const float A = art.a + art.b * Ptm;
		D = art.D * sqrt(A);
		const float vf = viscosityFactor(D, hct);
		Rs = 128*Kr/M_PI * vf * dL / sqr(sqr(D)) * art.vessel_ratio;
		
		viscosity += vf;
		volume += M_PI_F/4.0 * sqr(D_integral) * dL;
		D_integral += D;
	}

	result[vessel_index].Dmin = D;
	Pout = Pout + art.flow * Rs;
	Rtot += Rs;

	for (int sum=1; sum<nSums; sum++){
		Ptm = 1.35951002636 * ( Pout - art.tone ) - art.GP;
		Ptm = Ptm - art.Ppl - art.peri_a - art.peri_b * exp( art.peri_c * ( Ptm - art.Ppl ));
		
		const float A = art.a + art.b * Ptm;
		D = art.D * sqrt(A);
		D_integral += D;
		const float vf = viscosityFactor(D, hct);
		Rs = 128.0*Kr/M_PI * vf * dL / sqr(sqr(D)) * art.vessel_ratio;
		
		viscosity += vf;
		volume += M_PI_F/4.0 * sqr(D) * dL;

		Pout += art.flow * Rs;
		Rtot += Rs;
	}

	result[vessel_index].R = Rtot;
	result[vessel_index].D = D_integral / nSums;
	result[vessel_index].Dmax = D;
	result[vessel_index].volume = volume / (1e9 * art.vessel_ratio);
	result[vessel_index].viscosity_factor = viscosity / nSums;
}

__kernel void integrateOutsideArtery(
                float hct,
                __global __read_only struct Vessel *v,
                __global __write_only struct Result *result)
{
	const size_t vessel_index = get_global_id(0) + get_global_id(1)*WIDTH;
	const struct Vessel art = v[vessel_index];

	float Rtot = 0.0;
	float Pout = art.pressure;

	float Ptm = 1.35951002636 * ( Pout - art.tone ) - art.GP;
	float Rs;

	float D = 0.0;
	float D_integral = 0.0;
	float volume = 0.0;
	float viscosity = 0.0;
	float dL = art.len / nSums;

	Ptm = Ptm - art.Ppl;
	if( Ptm < 0 ) {
		Rs = -Ptm/( 1.35951002636 * art.flow ); // Starling Resistor
	}
	else {
		const float A = art.a + art.b * Ptm;
		D = art.D * sqrt(A);
		const float vf = viscosityFactor(D, hct);
		Rs = 128*Kr/M_PI * vf * dL / sqr(sqr(D)) * art.vessel_ratio;

		viscosity += vf;
		volume += M_PI_F/4.0 * sqr(D) * dL;
		D_integral += D;
	}

	result[vessel_index].Dmin = D;
	Pout = Pout + art.flow * Rs;
	Rtot += Rs;

	for (int sum=1; sum<nSums; sum++){
		Ptm = 1.35951002636 * ( Pout - art.tone ) - art.GP;
		Ptm = Ptm - art.Ppl;
		
		const float A = art.a + art.b * Ptm;
		D = art.D * sqrt(A);
		D_integral += D;
		const float vf = viscosityFactor(D, hct);
		Rs = 128*Kr/M_PI * vf * dL / sqr(sqr(D)) * art.vessel_ratio;
		
		viscosity += vf;
		volume += M_PI_F/4.0 * sqr(D) * dL;

		Pout += art.flow * Rs;
		Rtot += Rs;
	}

	result[vessel_index].R = Rtot;
	result[vessel_index].D = D_integral / nSums;
	result[vessel_index].Dmax = D;
	result[vessel_index].volume = volume / (1e9 * art.vessel_ratio);
	result[vessel_index].viscosity_factor = viscosity / nSums;
}

__kernel void integrateInsideVein(
                float hct,
                __global __read_only struct Vessel *v, 
                __global __read_only float *next_vein_pressure, 
                __global __write_only struct Result *result)
{
	const size_t vessel_index = get_global_id(0) + get_global_id(1)*WIDTH;
	const struct Vessel vein = v[vessel_index];

	float Rtot = 0.0;
	float Pout = next_vein_pressure[vessel_index]; // true for first vein only

	float Ptm = 1.35951002636 * ( Pout - vein.tone ) - vein.GP;
	float Rs;

	float D = 0.0;
	float D_integral = 0.0;
	float volume = 0.0;
	float viscosity = 0.0;
	float dL = vein.len / nSums;

	Ptm = Ptm - vein.Ppl - vein.peri_a - vein.peri_b * exp( vein.peri_c * ( Ptm - vein.Ppl ));

	if( Ptm < 0 ) {
		Rs = -Ptm/( 1.35951002636 * vein.flow ); // Starling Resistor
	}
	else {
		const float inv_A = 1.0 + vein.b * exp( vein.c * Ptm );
		D = vein.D * rsqrt(inv_A);
		const float vf = viscosityFactor(D, hct);
		Rs = 128*Kr/M_PI * vf * dL / sqr(sqr(D)) * vein.vessel_ratio;
		
		D_integral = D;
		viscosity += vf;
		volume += M_PI_F/4.0 * sqr(D) * dL;
	}

	result[vessel_index].Dmin = D;
	Pout = Pout + vein.flow * Rs;
	Rtot = Rs;

	for( int sum=1; sum<nSums; sum++ ){
		Ptm = 1.35951002636 * ( Pout - vein.tone ) - vein.GP;
		Ptm = Ptm - vein.Ppl - vein.peri_a - vein.peri_b * exp( vein.peri_c * ( Ptm - vein.Ppl ));

		const float inv_A = 1.0 + vein.b * exp( vein.c * Ptm );
		D = vein.D * rsqrt(inv_A);
		const float vf = viscosityFactor(D, hct);
		Rs = 128*Kr/M_PI * vf * dL / sqr(sqr(D)) * vein.vessel_ratio;

		volume += M_PI_F/4.0 * sqr(D) * dL;
		viscosity += vf;
		D_integral += D;
		
		Pout = Pout + vein.flow * Rs;
		Rtot += Rs;
	}

	result[vessel_index].R = Rtot;
	result[vessel_index].D = D_integral / nSums;
	result[vessel_index].Dmax = D;
	result[vessel_index].volume = volume / (1e9 * vein.vessel_ratio);
	result[vessel_index].viscosity_factor = viscosity / nSums;
}

__kernel void integrateOutsideVein(
                float hct,
                __global __read_only struct Vessel *v, 
                __global __read_only float *next_vein_pressure, 
                __global __write_only struct Result *result)
{
	const size_t vessel_index = get_global_id(0) + get_global_id(1)*WIDTH;
	const struct Vessel vein = v[vessel_index];

	float Rtot = 0.0;
	float Pout = next_vein_pressure[vessel_index];

	float Ptm = 1.35951002636 * ( Pout - vein.tone ) - vein.GP;
	float Rs;

	float D = 0.0;
	float D_integral = 0.0;
	float volume = 0.0;
	float viscosity = 0.0;
	float dL = vein.len / nSums;

	Ptm = Ptm - vein.Ppl;

	if( Ptm < 0 ) {
		Rs = -Ptm/( 1.35951002636 * vein.flow ); // Starling Resistor
	}
	else {
		const float inv_A = 1.0 + vein.b * exp( vein.c * Ptm );
		D = vein.D * rsqrt(inv_A);
		const float vf = viscosityFactor(D, hct);
		Rs = 128*Kr/M_PI * vf * dL / sqr(sqr(D)) * vein.vessel_ratio;
		
		D_integral = D;
		viscosity += vf;
		volume += M_PI_F/4.0 * sqr(D) * dL;
	}

	result[vessel_index].Dmin = D;
	Pout = Pout + vein.flow * Rs;
	Rtot = Rs;

	for( int sum=1; sum<nSums; sum++ ){
		Ptm = 1.35951002636 * ( Pout - vein.tone ) - vein.GP;
		Ptm = Ptm - vein.Ppl;

		const float A = 1.0 + vein.b * exp( vein.c * Ptm );
		D = vein.D * rsqrt(A);
		const float vf = viscosityFactor(D, hct);
		Rs = 128*Kr/M_PI * vf * dL / sqr(sqr(D)) * vein.vessel_ratio;

		volume += M_PI_F/4.0 * sqr(D) * dL;
		viscosity += vf;
		D_integral += D;
		
		Pout = Pout + vein.flow * Rs;
		Rtot += Rs;
	}

	result[vessel_index].R = Rtot;
	result[vessel_index].D = D_integral / nSums;
	result[vessel_index].Dmax = D;
	result[vessel_index].volume = volume / (1e9 * vein.vessel_ratio);
	result[vessel_index].viscosity_factor = viscosity / nSums;
}
