struct Vessel {
	float R;
	float pressure_in;
	float pressure_out;
	float GP;
	float tone;
	float flow;
	float Ppl;

	float max_a;
	float gamma;
	float phi;
	float c;

	float peri_a;
	float peri_b;
	float peri_c;
	float P_0;

	float D;
	float len;

	float vessel_ratio;

	float pad[14 + 2*16]; // pad to 256 bytes
};

struct Result {
	float R;
	float delta_R;
	float D;
	float Dmin;
	float Dmax;
	float volume;
	float viscosity_factor;

	float pad[1+8 + 3*16]; // pad to 256 bytes
};

__constant float Kr = 1.2501e8; // cP/um**3 => mmHg*min/l
__constant int nSums = 128;
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

__kernel void singleSegmentVesselFlow(
		int width,
		float hct,
		float tlrns,
		__global struct Vessel *v,
		__global struct Result *result)
{
	/* NOTE: calc_dim is assumed empty, if supplied */
	const size_t vessel_index = get_global_id(0) + get_global_id(1)*width;
	const struct Vessel vein = v[vessel_index];
	
	const float Rin = vein.R;
	const float Pin = vein.pressure_in;
	const float Pout = fmax(vein.pressure_out, vein.P_0);
	const float starling_R = (vein.P_0 - fmin(vein.pressure_out, vein.P_0)) / vein.flow;

	float Ptm = 1.35951002636 * ((Pin+Pout)/2.0 - vein.tone);
	float Rs;

	/* Conditions like no flow, isnan(P), etc. are weeded out before
	 * vessels are passed to OpenCL
	 */

	// check if vessel is closed
	if (vein.D < 0.1) {
		result[vessel_index].D = 0.0;
		result[vessel_index].Dmin = 0.0;
		result[vessel_index].Dmax = 0.0;

		result[vessel_index].viscosity_factor = INFINITY;
		result[vessel_index].volume = 0.0;
		result[vessel_index].R = INFINITY;
		result[vessel_index].delta_R = 0.0;
		return;
	}

	Ptm = Ptm - vein.Ppl - vein.peri_a - vein.peri_b * exp( vein.peri_c * ( Ptm - vein.Ppl ));

	float D = 0.0;
	float vf = 0.0;
	
	float new_Pin = Pin;
	float old_Pin;
		
	do {
		old_Pin = new_Pin;
		float avg_P = (new_Pin + Pout) / 2.0;
		Ptm = 1.35951002636 * ( avg_P - vein.tone );
		Ptm = Ptm - vein.Ppl - vein.peri_a -
		      vein.peri_b * exp( vein.peri_c * ( Ptm - vein.Ppl ));
		
		D = vein.D*vein.gamma - (vein.gamma-1.0)*vein.D*exp(-Ptm*vein.phi/(vein.gamma-1.0));
		vf = viscosityFactor(D, hct);
		Rs = 128*Kr/M_PI * vf * vein.len / sqr(sqr(D)) * vein.vessel_ratio;

		new_Pin = Pout + vein.flow * Rs;
		new_Pin = (new_Pin + old_Pin) / 2.0;
	} while (fabs(new_Pin - old_Pin)/old_Pin > tlrns);
	
	Rs += starling_R;
	
	result[vessel_index].viscosity_factor = vf;
	result[vessel_index].volume = M_PI/4.0 * sqr(D) * vein.len / (1e9*vein.vessel_ratio); // um**3 => uL, and correct for real number of vessels
	result[vessel_index].Dmax = D;
	result[vessel_index].Dmin = D;
	result[vessel_index].D = D;
	result[vessel_index].R = Rs;
	result[vessel_index].delta_R = fabs(Rin - Rs)/Rin;
}

__kernel void multiSegmentedVesselFlow(
		int width,
                float hct,
		float tlrns,
                __global struct Vessel *v, 
                __global struct Result *result)
{
	const size_t vessel_index = get_global_id(0) + get_global_id(1)*width;
	const struct Vessel vein = v[vessel_index];

	float Pout = fmax(vein.pressure_out, vein.P_0);
	const float starling_R = (vein.P_0 - fmin(vein.pressure_out, vein.P_0)) / vein.flow;
	float Rtot = 0.0;

	float D;
	float D_integral = 0.0;
	float volume = 0.0;
	float viscosity = 0.0;
	float dL = vein.len / (float)nSums;

	/* Conditions like no flow, isnan(P), etc. are weeded out before
	 * vessels are passed to OpenCL
	 */

	// check if vessel is closed
	if (vein.D < 0.1) {
		result[vessel_index].D = 0.0;
		result[vessel_index].Dmin = 0.0;
		result[vessel_index].Dmax = 0.0;

		result[vessel_index].viscosity_factor = INFINITY;
		result[vessel_index].volume = 0.0;
		result[vessel_index].R = INFINITY;
		result[vessel_index].delta_R = 0.0;
		return;
	}

	for( int sum=0; sum<nSums; sum++ ){
		const float Ptm_i = 1.35951002636 * ( Pout - vein.tone );
		const float Ptm = Ptm_i - vein.Ppl - vein.peri_a - 
				vein.peri_b * exp( vein.peri_c * ( Ptm_i - vein.Ppl ));
		D = vein.D*vein.gamma - (vein.gamma-1.0)*vein.D*exp(-Ptm*vein.phi/(vein.gamma-1.0));
		const float vf = viscosityFactor(D, hct);

		const float Rs = 128*Kr/M_PI * vf * dL / sqr(sqr(D)) * vein.vessel_ratio;

		volume += M_PI_F/4.0 * sqr(D) * dL;
		viscosity += vf;
		D_integral += D;

		if (sum == 0)
			result[vessel_index].Dmin = D;
		
		Pout = Pout + vein.flow * Rs;
		Rtot += Rs;
	}

	Rtot += starling_R;
	result[vessel_index].R = Rtot;
	result[vessel_index].D = D_integral / nSums;
	result[vessel_index].Dmax = D;
	result[vessel_index].volume = volume / (1e9 * vein.vessel_ratio);
	result[vessel_index].viscosity_factor = viscosity / nSums;
	result[vessel_index].delta_R = fabs(vein.R-Rtot)/vein.R;
}

