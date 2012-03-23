({

name: function() {
	return ["PAH", "Pulmonary Arterial Hypertension"];
},

parameters: function() {
	return [["Compromise", "0 to 100"]];
},

artery: function(compromise) {
	if (this.gen > 4*this.n_gen/5) {
		var area = (100.0 - compromise)/100.0;
		this.R = this.R/area/area;
		return true;
	}

	return false;
},

})
