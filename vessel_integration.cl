#define nSums 8192
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

	float Kz;

	float D;
	float len;
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

inline float sqr(float n)
{
	return n*n;
}

inline float viscosityFactor(float D, float Hct)
{
	return 1.0;

	const float C = (0.8+exp(-0.075*D)) * ((1/(1 + 1e-11*pown(D, 12)))-1.0) + (1/(1+1e-11*pown(D,12)));
	const float Mi45 = 220 * exp(-1.3*D) - 2.44*exp(-0.06*powr(D, 0.645)) + 3.2;
	return (1.0 + (Mi45-1.0)*(powr(1.0-Hct, C)-1.0)/(powr(1.0-0.45, C)-1.0)) / 3.2;
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

	float Ptm = 1.36 * ( Pout - art.tone ) - art.GP;
	float Rs;

	float D = 0.0;
	float D_integral = 0.0;
	float volume = 0.0;
	float viscosity = 0.0;
	float dL = art.len / nSums;

	Ptm = Ptm - art.Ppl - art.peri_a - art.peri_b * exp( art.peri_c * ( Ptm - art.Ppl ));
	if( Ptm < 0 ) {
		Rs = -Ptm/( 1.36 * art.flow ); // Starling Resistor
	}
	else {
		const float A = art.a + art.b * Ptm;
		D = art.D * sqrt(A);
		const float vf = viscosityFactor(D_integral, hct);
		Rs = art.Kz * vf / sqr(A);
		
		viscosity += vf;
		volume += M_PI_F * sqr(D_integral / 2.0 / 1e4) * dL;
		D_integral += D;
	}

	result[vessel_index].Dmin = D;
	Pout = Pout + art.flow * Rs;
	Rtot += Rs;

	for (int sum=1; sum<nSums; sum++){
		Ptm = 1.36 * ( Pout - art.tone ) - art.GP;
		Ptm = Ptm - art.Ppl - art.peri_a - art.peri_b * exp( art.peri_c * ( Ptm - art.Ppl ));
		
		const float A = art.a + art.b * Ptm;
		D = art.D * sqrt(A);
		D_integral += D;
		const float vf = viscosityFactor(D, hct);
		Rs = art.Kz * vf / sqr(A);
		
		viscosity += vf;
		volume += M_PI_F * sqr(D/2.0/1e4) * dL;

		Pout += art.flow * Rs;
		Rtot += Rs;
	}
	Rtot = 0.5*Rtot + 0.5*art.R;

	result[vessel_index].R = Rtot;
	result[vessel_index].D = D_integral / nSums;
	result[vessel_index].Dmax = D;
	result[vessel_index].volume = volume;
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

	float Ptm = 1.36 * ( Pout - art.tone ) - art.GP;
	float Rs;

	float D = 0.0;
	float D_integral = 0.0;
	float volume = 0.0;
	float viscosity = 0.0;
	float dL = art.len / nSums;

	Ptm = Ptm - art.Ppl;
	if( Ptm < 0 ) {
		Rs = -Ptm/( 1.36 * art.flow ); // Starling Resistor
	}
	else {
		const float A = art.a + art.b * Ptm;
		D = art.D * sqrt(A);
		const float vf = viscosityFactor(D_integral, hct);
		Rs = art.Kz / sqr(A) * vf;

		viscosity += vf;
		volume += M_PI_F * sqr(D_integral / 2.0 / 1e4) * dL;
		D_integral += D;
	}

	result[vessel_index].Dmin = D;
	Pout = Pout + art.flow * Rs;
	Rtot += Rs;

	for (int sum=1; sum<nSums; sum++){
		Ptm = 1.36 * ( Pout - art.tone ) - art.GP;
		Ptm = Ptm - art.Ppl;
		
		const float A = art.a + art.b * Ptm;
		D = art.D * sqrt(A);
		D_integral += D;
		const float vf = viscosityFactor(D, hct);
		Rs = art.Kz * vf / sqr(A);
		
		viscosity += vf;
		volume += M_PI_F * sqr(D/2.0/1e4) * dL;

		Pout += art.flow * Rs;
		Rtot += Rs;
	}
	Rtot = 0.5*Rtot + 0.5*art.R;

	result[vessel_index].R = Rtot;
	result[vessel_index].D = D_integral / nSums;
	result[vessel_index].Dmax = D;
	result[vessel_index].volume = volume;
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

	float Ptm = 1.36 * ( Pout - vein.tone ) - vein.GP;
	float Rs;

	float D = 0.0;
	float D_integral = 0.0;
	float volume = 0.0;
	float viscosity = 0.0;
	float dL = vein.len / nSums;

	Ptm = Ptm - vein.Ppl - vein.peri_a - vein.peri_b * exp( vein.peri_c * ( Ptm - vein.Ppl ));

	if( Ptm < 0 ) {
		Rs = -Ptm/( 1.36 * vein.flow ); // Starling Resistor
	}
	else {
		const float A = 1.0 + vein.b * exp( vein.c * Ptm );
		D = vein.D / sqrt(A);
		const float vf = viscosityFactor(D, hct);
		Rs = vein.Kz * vf * sqr(A);
		
		D_integral = D;
		viscosity += vf;
		volume += M_PI_F * sqr(D / 2.0 / 1e4) * dL;
	}

	result[vessel_index].Dmin = D;
	Pout = Pout + vein.flow * Rs;
	Rtot = Rs;

	for( int sum=1; sum<nSums; sum++ ){
		Ptm = 1.36 * ( Pout - vein.tone ) - vein.GP;
		Ptm = Ptm - vein.Ppl - vein.peri_a - vein.peri_b * exp( vein.peri_c * ( Ptm - vein.Ppl ));

		const float A = 1.0 + vein.b * exp( vein.c * Ptm );
		D = vein.D * rsqrt(A);
		const float vf = viscosityFactor(D, hct);
		Rs = vein.Kz * vf * sqr(A);

		volume += M_PI_F * sqr(D/2/1e4) * dL;
		viscosity += vf;
		D_integral += D;
		
		Pout = Pout + vein.flow * Rs;
		Rtot += Rs;
	}

	Rtot = 0.5*Rtot + 0.5*vein.R;

	result[vessel_index].R = Rtot;
	result[vessel_index].D = D_integral / nSums;
	result[vessel_index].Dmax = D;
	result[vessel_index].volume = volume;
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

	float Ptm = 1.36 * ( Pout - vein.tone ) - vein.GP;
	float Rs;

	float D = 0.0;
	float D_integral = 0.0;
	float volume = 0.0;
	float viscosity = 0.0;
	float dL = vein.len / nSums;

	Ptm = Ptm - vein.Ppl;

	if( Ptm < 0 ) {
		Rs = -Ptm/( 1.36 * vein.flow ); // Starling Resistor
	}
	else {
		const float A = 1.0 + vein.b * exp( vein.c * Ptm );
		D = vein.D / sqrt(A);
		const float vf = viscosityFactor(D, hct);
		Rs = vein.Kz * vf * sqr(A);
		
		D_integral = D;
		viscosity += vf;
		volume += M_PI_F * sqr(D / 2.0 / 1e4) * dL;
	}

	result[vessel_index].Dmin = D;
	Pout = Pout + vein.flow * Rs;
	Rtot = Rs;

	for( int sum=1; sum<nSums; sum++ ){
		Ptm = 1.36 * ( Pout - vein.tone ) - vein.GP;
		Ptm = Ptm - vein.Ppl;

		const float A = 1.0 + vein.b * exp( vein.c * Ptm );
		D = vein.D * rsqrt(A);
		const float vf = viscosityFactor(D, hct);
		Rs = vein.Kz * vf * sqr(A);

		volume += M_PI_F * sqr(D/2/1e4) * dL;
		viscosity += vf;
		D_integral += D;
		
		Pout = Pout + vein.flow * Rs;
		Rtot += Rs;
	}

	Rtot = 0.5*Rtot + 0.5*vein.R;

	result[vessel_index].R = Rtot;
	result[vessel_index].D = D_integral / nSums;
	result[vessel_index].Dmax = D;
	result[vessel_index].volume = volume;
	result[vessel_index].viscosity_factor = viscosity / nSums;
}
