#define nSums 10000
#define WIDTH 4

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
	float pad[3]; // pad to 64 bytes
};

inline float sqr(float n)
{
	return n*n;
}

__kernel void integrateInsideArtery(
                   __global __read_only struct Vessel *v, 
                   __global __write_only float *result)
{
	const size_t vessel_index = get_global_id(0) + get_global_id(1)*WIDTH;
	const struct Vessel art = v[vessel_index];
	
	float Rtot = 0.0;
	float Pout = art.pressure;

	float Ptm = 1.36 * ( Pout - art.tone ) - art.GP;
	float Rs;

	Ptm = Ptm - art.Ppl - art.peri_a - art.peri_b * exp( art.peri_c * ( Ptm - art.Ppl ));
        if( Ptm < 0 )
                Rs = -Ptm/( 1.36 * art.flow ); // Starling Resistor
        else
                Rs = art.Kz / sqr( art.a + art.b * Ptm );

        Pout = Pout + art.flow * Rs;
        Rtot += Rs;

        for (int sum=1; sum<nSums; sum++){
                Ptm = 1.36 * ( Pout - art.tone ) - art.GP;
                Ptm = Ptm - art.Ppl - art.peri_a - art.peri_b * exp( art.peri_c * ( Ptm - art.Ppl ));
                Rs = art.Kz / sqr(art.a + art.b * Ptm);

                Pout += art.flow * Rs;
                Rtot += Rs;
        }
        Rtot = 0.5*Rtot + 0.5*art.R;
        result[vessel_index] = Rtot;
}

__kernel void integrateOutsideArtery(
                   __global __read_only struct Vessel *v,
                   __global __write_only float *result)
{
	const size_t vessel_index = get_global_id(0) + get_global_id(1)*WIDTH;
	const struct Vessel art = v[vessel_index];
	
	float Rtot = 0.0;
	float Pout = art.pressure;

	float Ptm = 1.36 * ( Pout - art.tone ) - art.GP;
	float Rs;

        Ptm = Ptm - art.Ppl;
        if( Ptm < 0 )
                Rs = -Ptm/( 1.36 * art.flow ); // Starling Resistor
        else
                Rs = art.Kz / sqr( art.a + art.b * Ptm );

        Pout = Pout + art.flow * Rs;
        Rtot += Rs;

        for (int sum=1; sum<nSums; sum++){
                Ptm = 1.36 * ( Pout - art.tone ) - art.GP;
                Ptm = Ptm - art.Ppl;
                Rs = art.Kz / sqr(art.a + art.b * Ptm);

                Pout += art.flow * Rs;
                Rtot += Rs;
        }
        Rtot = 0.5*Rtot + 0.5*art.R;
        result[vessel_index] = Rtot;
}

__kernel void integrateInsideVein(
                   __global __read_only struct Vessel *v, 
                   __global __read_only float *next_vein_pressure, 
                   __global __write_only float *result)
{
	const size_t vessel_index = get_global_id(0) + get_global_id(1)*WIDTH;
	const struct Vessel vein = v[vessel_index];

	float Rtot = 0.0;
	float Pout = next_vein_pressure[vessel_index]; // true for first vein only

	float Ptm = 1.36 * ( Pout - vein.tone ) - vein.GP;
	float Rs;

	Ptm = Ptm - vein.Ppl - vein.peri_a - vein.peri_b * exp( vein.peri_c * ( Ptm - vein.Ppl ));

	if( Ptm < 0 )
		Rs = -Ptm/( 1.36 * vein.flow ); // Starling Resistor
	else {
		Rs = vein.Kz * sqr(1.0 + vein.b * exp( vein.c * Ptm ));
	}


	Pout = Pout + vein.flow * Rs;
	Rtot = Rs;

	for( int sum=1; sum<nSums; sum++ ){
		Ptm = 1.36 * ( Pout - vein.tone ) - vein.GP;
		Ptm = Ptm - vein.Ppl - vein.peri_a - vein.peri_b * exp( vein.peri_c * ( Ptm - vein.Ppl ));
		Rs = vein.Kz * sqr(1.0 + vein.b * exp( vein.c * Ptm ));

		Pout = Pout + vein.flow * Rs;
		Rtot += Rs;
	}

	Rtot = 0.5*Rtot + 0.5*vein.R;
	result[vessel_index] = Rtot;
}

__kernel void integrateOutsideVein(
                   __global __read_only struct Vessel *v, 
                   __global __read_only float *next_vein_pressure, 
                   __global __write_only float *result)
{
	const size_t vessel_index = get_global_id(0) + get_global_id(1)*WIDTH;
	const struct Vessel vein = v[vessel_index];

	float Rtot = 0.0;
	float Pout = next_vein_pressure[vessel_index];

	float Ptm = 1.36 * ( Pout - vein.tone ) - vein.GP;
	float Rs;

	Ptm = Ptm - vein.Ppl;

	if( Ptm < 0 )
		Rs = -Ptm/( 1.36 * vein.flow ); // Starling Resistor
	else
		Rs = vein.Kz * sqr(1.0 + vein.b * exp( vein.c * Ptm ));


	Pout = Pout + vein.flow * Rs;
	Rtot = Rs;

	for( int sum=1; sum<nSums; sum++ ){
		Ptm = 1.36 * ( Pout - vein.tone ) - vein.GP;
		Ptm = Ptm - vein.Ppl;
		Rs = vein.Kz * sqr(1.0 + vein.b * exp( vein.c * Ptm ));

		Pout = Pout + vein.flow * Rs;
		Rtot += Rs;
	}

	Rtot = 0.5*Rtot + 0.5*vein.R;
	result[vessel_index] = Rtot;
}

