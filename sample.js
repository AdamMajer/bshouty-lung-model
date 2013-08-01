({

name: function() {
	/* Returns one of,
         *    A single string identifying the name of the disease, or
         *    Two strings, in an array. The first element as above, and the 2nd
         *    element as a long description of this model
         */
	return "Enter Disease Name Here";

        // return ["Short", "And long description of the model is here"];
},

parameters: function() {
	/* Returns parameter names that are then passed to artery,
	 * vein and cap functions as arguments, along with the ranges.
	 * There may be as many parameters, or as few (including none),
	 * as required.
         * If a range is not specified, then it is NOT possible to
         * estimate disease parameters based on outcome values.
         *
         * Return format is array of arrays, one per parameter
         * Each paramter array constitues of
         *     [paramter short name, 
         *      paramter range, 
         *      default value (optional), 
         *      parameter description (optional)]
	 */
	return [["param1", "0 to 100"],
                ["param2", "-5 to -1", 5, "Description for param2 is here"]];
},

/* The following global values are available to all of the following
   functions,

	this.Lung_Ht
	this.Flow
	this.LAP
	this.Pal
	this.Ppl
	this.Ptp
	this.PAP
	this.Tlrns
	this.MV
	this.CL
	this.Pat_Ht
	this.Pat_Wt
	this.n_gen - total number of generations
*/

artery: function(param1, param2) {
	/* This function sets values associated
	   with each artery. The following values are available
	   and may be modifed by this function,
		this.gen - generation no of the vessel, READ ONLY
		this.vessel_idx - vessel number in a generation, READ ONLY
		this.R
		this.gamma
		this.phi
		this.c
		this.tone
		this.GP
		this.Ppl
		this.Ptp
		this.perivascular_press_a
		this.perivascular_press_b
		this.perivascular_press_c
		this.Kz
	*/

	return false;
},

vein: function(param1, param2) {
	/* This function sets values associated
	   with each vein. The following values are available
	   and may be modifed by this function,
		this.gen - generation no of the vessel, READ ONLY
		this.vessel_idx - vessel number in a generation, READ ONLY
		this.R
		this.gamma
		this.phi
		this.c
		this.tone
		this.GP
		this.Ppl
		this.Ptp
		this.perivascular_press_a
		this.perivascular_press_b
		this.perivascular_press_c
		this.Kz
	  Return 'true' if values are modified, 'false' otherwise
	*/

	return false;
},

cap: function(param1, param2) {
	/* This function sets values associated
	   with each artery. The following values are available
	   and may be modifed by this function,
		this.vessel_idx - vessel number, READ ONLY
		this.Krc
		this.Alpha
		this.Ho
	*/

	return false;
}


})

