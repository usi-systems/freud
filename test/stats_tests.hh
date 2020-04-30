/*
struct corr_with_constraints {
        std::string path_conditions;
        std::vector<struct path_filter> path_conditions_vec;
        bool constant;
        int constant_value;
        multiple_regression* reg;
        bool addnoise_removed;
        bool main_trend;
        metric_type metric;
        feature_type feature;
        std::vector<measure *> data_points;
};

struct path_filter {
	std::string feature;
	filter_type flt;
	int lower_bound;
	int upper_bound;
	double probability;
	double cl_lower_bound;
	double cl_upper_bound;
};

*/

bool check_ann_Z22test_random_clusteringi(std::map<std::string, annotation *> m_ann_map) {
	// Annotation exists
	const std::string mname = "_Z22test_random_clusteringi";

	assert(m_ann_map.find(mname) != m_ann_map.end());

	annotation * a = m_ann_map[mname];
	
	// Correct type 
	assert(a->regressions.size() == 0);
	assert(a->clusters.size() >= 3);
	
	std::cout << "\033[1;32m Success! \033[0m" << std::endl;

	// TODO: check some properties of the clusters?
	return false;
}

bool check_ann_Z18test_linear_farrayPA10_j(std::map<std::string, annotation *> m_ann_map) {
	std::cout << "\033[1;32m Array disabled! \033[0m" << std::endl;
	return false;
	// Annotation exists
	const std::string mname = "_Z18test_linear_farrayPA10_j";

	assert(m_ann_map.find(mname) != m_ann_map.end());

	annotation * a = m_ann_map[mname];
	
	// Correct type 
	assert(a->clusters.size() == 0);
	assert(a->regressions.size() == 1);
	
	corr_with_constraints cwc = a->regressions[0];

	// Annotation type and scope
	assert(cwc.constant == false);
	assert(cwc.addnoise_removed == false);
	assert(cwc.main_trend == false);
	assert(cwc.path_conditions_vec.size() == 0);

	// Regression
	multiple_regression* r = cwc.reg;
	// Regression Feature 
	assert(r->features.size() == 1);
 	assert(r->features[0] == "artificial_arr_aggr");
	assert(r->determination > 0.9); // leave this?
	// Coefficients
	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}

bool check_ann_Z18test_linear_vectorPSt6vectorIiSaIiEE(std::map<std::string, annotation *> m_ann_map) {
	// Annotation exists
	const std::string mname = "_Z18test_linear_vectorPSt6vectorIiSaIiEE";

	assert(m_ann_map.find(mname) != m_ann_map.end());

	annotation * a = m_ann_map[mname];
	
	// Correct type 
	assert(a->clusters.size() == 0);
	assert(a->regressions.size() == 1);
	
	corr_with_constraints cwc = a->regressions[0];

	// Annotation type and scope
	assert(cwc.constant == false);
	assert(cwc.addnoise_removed == false);
	assert(cwc.main_trend == false);
	assert(cwc.path_conditions_vec.size() == 0);

	// Regression
	multiple_regression* r = cwc.reg;
	// Linear
	assert(r->features.size() == 1);
 	assert(r->features[0] == "t.artificial.size");
	assert(r->determination > 0.9); // leave this?
	// Coefficients
	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}

bool check_ann_Z20test_linear_branchesiii(std::map<std::string, annotation *> m_ann_map) {
	// Annotation exists
	const std::string mname = "_Z20test_linear_branchesiii";

	assert(m_ann_map.find(mname) != m_ann_map.end());

	annotation * a = m_ann_map[mname];
	
	// Correct type 
	assert(a->clusters.size() == 0);
	assert(a->regressions.size() == 2);
	
	// FIRST REGRESSION
	corr_with_constraints cwc = a->regressions[0];

	// Annotation type and scope
	assert(cwc.constant == false);
	assert(cwc.addnoise_removed == false);
	assert(cwc.main_trend == false);
	assert(cwc.path_conditions_vec.size() == 1);
	struct path_filter pf = cwc.path_conditions_vec[0];
	assert(pf.feature == "a");
	assert(pf.flt == FLT_LE);
	assert(pf.upper_bound >= 0 && pf.upper_bound <= 10); 

	// Linear Regression
	multiple_regression* r = cwc.reg;
	assert(r->features.size() == 1);
 	assert(r->features[0] == "c");
	assert(r->determination > 0.9); // leave this?
	// Coefficients

	// SECOND REGRESSION
	cwc = a->regressions[1];

	// Annotation type and scope
	assert(cwc.constant == false);
	assert(cwc.addnoise_removed == false);
	assert(cwc.main_trend == false);
	assert(cwc.path_conditions_vec.size() == 1);
	pf = cwc.path_conditions_vec[0];
	assert(pf.feature == "a");
	assert(pf.flt == FLT_GT);
	assert(pf.lower_bound >= 0 && pf.lower_bound <= 10); 

	// Regression
	r = cwc.reg;
	// Quadratic
	assert(r->features.size() >= 1); // system features
 	assert(r->features[0] == "b^2");
	assert(r->determination > 0.9); // leave this?
	// Coefficients

	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}

bool check_ann_Z25test_linear_fitinregister15fit_in_register(std::map<std::string, annotation *> m_ann_map) {
	// Annotation exists
	const std::string mname = "_Z25test_linear_fitinregister15fit_in_register";

	assert(m_ann_map.find(mname) != m_ann_map.end());

	annotation * a = m_ann_map[mname];
	
	// Correct type 
	assert(a->clusters.size() == 0);
	assert(a->regressions.size() == 1);
	
	corr_with_constraints cwc = a->regressions[0];

	// Annotation type and scope
	assert(cwc.constant == false);
	assert(cwc.addnoise_removed == false);
	assert(cwc.main_trend == false);
	assert(cwc.path_conditions_vec.size() == 0);

	// Regression
	multiple_regression* r = cwc.reg;
#if 0
	// Linear
	assert(r->degree == 1);
	// Not n*log(n)
	assert(r->isnlogn == false);
	assert(r->var_name == "fir.single_int");
	assert(r->determination > 0.9); // leave this?
	assert(r->get_fkeyidx() == "0|11");
	// Coefficients
#endif
	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}

bool check_ann_Z19test_linear_classesP11basic_class(std::map<std::string, annotation *> m_ann_map) {
	// Annotation exists
	const std::string mname = "_Z19test_linear_classesP11basic_class";

	assert(m_ann_map.find(mname) != m_ann_map.end());

	annotation * a = m_ann_map[mname];
	
	// Correct type 
	assert(a->clusters.size() == 0);
	assert(a->regressions.size() == 1);
	
	corr_with_constraints cwc = a->regressions[0];

	// Annotation type and scope
	assert(cwc.constant == false);
	assert(cwc.addnoise_removed == false);
	assert(cwc.main_trend == false);
	assert(cwc.path_conditions_vec.size() == 0);

	// Regression
	multiple_regression* r = cwc.reg;
#if 0
	// Linear
	assert(r->degree == 1);
	// Not n*log(n)
	assert(r->isnlogn == false);
	assert(r->var_name == "bc.private_field");
	assert(r->determination > 0.9); // leave this?
	assert(r->get_fkeyidx() == "0|11");
	// Coefficients
#endif
	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}

bool check_ann_Z19test_linear_structsP15basic_structure(std::map<std::string, annotation *> m_ann_map) {
	// Annotation exists
	const std::string mname = "_Z19test_linear_structsP15basic_structure";

	assert(m_ann_map.find(mname) != m_ann_map.end());

	annotation * a = m_ann_map[mname];
	
	// Correct type 
	assert(a->clusters.size() == 0);
	assert(a->regressions.size() == 1);
	
	corr_with_constraints cwc = a->regressions[0];

	// Annotation type and scope
	assert(cwc.constant == false);
	assert(cwc.addnoise_removed == false);
	assert(cwc.main_trend == false);
	assert(cwc.path_conditions_vec.size() == 0);

	// Regression
	multiple_regression* r = cwc.reg;
	assert(r->features.size() == 1);
 	assert(r->features[0] == "bs.useful");
	assert(r->determination > 0.9); // leave this?
	// Coefficients
	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}

bool check_ann_Z19test_linear_charptrPc(std::map<std::string, annotation *> m_ann_map) {
	// Annotation exists
	const std::string mname = "_Z19test_linear_charptrPc";

	assert(m_ann_map.find(mname) != m_ann_map.end());

	annotation * a = m_ann_map[mname];
	
	// Correct type 
	assert(a->clusters.size() == 0);
	assert(a->regressions.size() == 1);
	
	corr_with_constraints cwc = a->regressions[0];

	// Annotation type and scope
	assert(cwc.constant == false);
	assert(cwc.addnoise_removed == false);
	assert(cwc.main_trend == false);
	assert(cwc.path_conditions_vec.size() == 0);

	// Regression
	multiple_regression* r = cwc.reg;
#if 0
	// Linear
	assert(r->degree == 1);
	// Not n*log(n)
	assert(r->isnlogn == false);
	assert(r->var_name == "str");
	assert(r->determination > 0.9); // leave this?
	assert(r->get_fkeyidx() == "0|4");
	// Coefficients
#endif
	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}

bool check_ann_Z16test_quad_int_wnii(std::map<std::string, annotation *> m_ann_map) {
	// Annotation exists
	const std::string mname = "_Z16test_quad_int_wnii";

	assert(m_ann_map.find(mname) != m_ann_map.end());

	annotation * a = m_ann_map[mname];
	
	// Correct type 
	assert(a->clusters.size() == 0);
	assert(a->regressions.size() == 1);
	
	corr_with_constraints cwc = a->regressions[0];

	// Annotation type and scope
	assert(cwc.constant == false);
	assert(cwc.addnoise_removed == false);
	assert(cwc.main_trend == false);
	assert(cwc.path_conditions_vec.size() == 0);

	// Regression
	multiple_regression* r = cwc.reg;
#if 0
	// Quadratic 
	assert(r->degree == 2);
	// Not n*log(n)
	assert(r->isnlogn == false);
	assert(r->var_name == "t");
	assert(r->determination > 0.9); // leave this?
	assert(r->get_fkeyidx() == "0|1");
	// Coefficients
#endif
	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}

bool check_ann_Z13test_quad_inti(std::map<std::string, annotation *> m_ann_map) {
	// Annotation exists
	const std::string mname = "_Z13test_quad_inti";

	assert(m_ann_map.find(mname) != m_ann_map.end());

	annotation * a = m_ann_map[mname];
	
	// Correct type 
	assert(a->clusters.size() == 0);
	assert(a->regressions.size() == 1);
	
	corr_with_constraints cwc = a->regressions[0];

	// Annotation type and scope
	assert(cwc.constant == false);
	assert(cwc.addnoise_removed == false);
	assert(cwc.main_trend == false);
	assert(cwc.path_conditions_vec.size() == 0);

	// Regression
	multiple_regression* r = cwc.reg;
#if 0
	// Quadratic 
	assert(r->degree == 2);
	// Not n*log(n)
	assert(r->isnlogn == false);
	assert(r->var_name == "t");
	assert(r->determination > 0.9); // leave this?
	assert(r->get_fkeyidx() == "0|1");
	// Coefficients
#endif
	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}

bool check_ann_Z15test_linear_inti(std::map<std::string, annotation *> m_ann_map) {
	// Annotation exists
	const std::string mname = "_Z15test_linear_inti";

	assert(m_ann_map.find(mname) != m_ann_map.end());

	annotation * a = m_ann_map[mname];
	
	// Correct type 
	assert(a->clusters.size() == 0);
	assert(a->regressions.size() == 1);
	
	corr_with_constraints cwc = a->regressions[0];

	// Annotation type and scope
	assert(cwc.constant == false);
	assert(cwc.addnoise_removed == false);
	assert(cwc.main_trend == false);
	assert(cwc.path_conditions_vec.size() == 0);

	// Regression
	multiple_regression* r = cwc.reg;
#if 0
	// Linear
	assert(r->degree == 1);
	// Not n*log(n)
	assert(r->isnlogn == false);
	assert(r->var_name == "t");
	assert(r->determination > 0.9); // leave this?
	assert(r->get_fkeyidx() == "0|1");
	// Coefficients
#endif
	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}


