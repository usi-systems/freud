#include "checker.hh"


checker::checker(std::string mname, method * m, metric_type mtype, bool remove_dependencies, annotation * performance_annotation, std::ofstream * out) {
	metric = mtype;
	mtd = m;
	mtd->name = mname;
	pa = performance_annotation;
	out_file = out;
}

void checker::filter_data(corr_with_constraints cwc, std::vector<measure *> in, std::vector<measure *> &out) {
	bool filtered = false;
	// If the PC is based on clustering, skip this phase, the data has already been filtered along with the cluster checks
	if (cwc.path_conditions_vec.size() > 0 && cwc.path_conditions_vec[0].feature != "CL") { 
		utils::get_filtered_data(cwc, in, out);
		std::cout << "Filtered on pc: " << in.size() << " - " << out.size() << std::endl;
		in = out;
		filtered = true;
	}
	
	printf("TODO: reimplement the checker\n");
	exit(-1);
	/*
	if (cwc.main_trend) {
		utils::get_main_trend(cwc.reg->get_fkeyidx(), metric, in, out);
		filtered = true;
	} else if (cwc.addnoise_removed) {
		utils::remove_addnoise(cwc.reg->get_fkeyidx(), metric, in, out);
		filtered = true;
	}

	if (!filtered)
		out = in;

	if (out.size() == 0)
		std::cout << "ERR: out is empty in filter_data!" << std::endl;
	*/
}

bool checker::validate_annotation() {
	printf("TODO: reimplement the checker\n");
	exit(-1);
	return false;
	/*
	if (pa->regressions.size() + pa->clusters.size() == 0) {
		(*out_file) << "Empty annotation read!" << std::endl;
		return true; //false;
	}

	bool result = true;
	std::cout << "Ann with " << pa->regressions.size() << "R and " << pa->clusters.size() << "C" << std::endl;

	for (corr_with_constraints cwc: pa->regressions) {
		//if (result == false)
		//	break;

		bool v = false;
		std::vector<measure *> data = mtd->data, f_data;

		if (cwc.path_conditions_vec.size() && cwc.path_conditions_vec[0].feature == "CL" && cwc.path_conditions_vec[0].flt == FLT_EQ) {

			// #########   REGRESSIONS AFTER CLUSTERS  #########	
			v = stats::validate_cluster_before_regression(mtd->data, metric, cwc.path_conditions_vec[0].probability, cwc.path_conditions_vec[0].cl_lower_bound, cwc.path_conditions_vec[0].cl_upper_bound, data);	
			result = result && v;
			//std::cout << "Validation1: " << v << " - " << result << std::endl;
		}

		// #########   REGRESSIONS   #########	
		//std::cout << "PFilters: " << cwc.path_conditions_vec.size() << std::endl;
		if (cwc.main_trend || cwc.addnoise_removed || cwc.path_conditions_vec.size() > 0) {
			// filter the data
			filter_data(cwc, data, f_data);
		} else {
			f_data = data;
		} 

		if (f_data.size() <= MAX_DEGREE + 1) {
			// Not enough samples to check this
			(*out_file) << "Skipping, not enough samples (" << f_data.size() << ")" << std::endl;
			continue;
		}

		if  (cwc.constant) {
			v = stats::validate_constant(f_data, metric, cwc.constant_value);
		} else {
			univariate_regression * r = cwc.reg;
			v = stats::validate_regression(f_data, r->get_fkeyidx(), metric, r);
		}
		//std::cout << "Validation: " << v << std::endl;
		(*out_file) << "Validation: " << v << " -- (" << f_data.size() << ")" << std::endl;
		result = result && v;
	}

	// #########   CLUSTERS   #########
	if (pa->clusters.size() > 0) {
		result = result && stats::validate_clusters(mtd->data, metric, pa->clusters);
	}

	return result;
	*/
}
