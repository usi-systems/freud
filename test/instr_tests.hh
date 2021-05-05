#define SYS_FEATURES 1
#define VPTR_FEATURES 1

bool check_Z18test_linear_farrayPA10_j(std::map<std::string, method *> &data) {
	std::cout << "\033[1;32m TODO: ARRAYS DISABLED! \033[0m" << std::endl;
	return false;

	// Data exists
	const std::string mname = "_Z18test_linear_farrayPA10_j";

	assert(data.find(mname) != data.end());

	// Correct and complete features
	method * m = data[mname];
	assert(m->feature_set.size() >= 3 && m->feature_set.size() <= 3 + SYS_FEATURES);

	assert(m->feature_set.find("arr") != m->feature_set.end());
	assert(m->feature_set.find("arr1") != m->feature_set.end());
	assert(m->feature_set.find("global_feature") != m->feature_set.end());

	// Check the actual log
	assert(m->data.size() == 1);
	
	measure * mm = m->data[0];
	assert(mm->features_map.size() >= 3 && mm->features_map.size() <= 3 + SYS_FEATURES);

	// This is actually correct from the dwarf pov... 
	// The first dimension of the array is not exposed
	// so it is seen as a arr[10], and not as arr[2][10] as in the code 
	assert(mm->get_feature_value("arr") == 10);		
	//assert(mm->get_feature_value("0|2") == 20);		

	// Same thing here, read above
	assert(mm->get_feature_value("arr1") == 170);		
	//assert(mm->get_feature_value("0|3") == 340);		
	
	assert(mm->get_feature_value("global_feature") == 55);
		
	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}

bool check_Z18test_linear_vectorPSt6vectorIiSaIiEE(std::map<std::string, method *> &data) {
	// Data exists
	const std::string mname = "_Z18test_linear_vectorPSt6vectorIiSaIiEE";

	assert(data.find(mname) != data.end());

	// Correct and complete features
	method * m = data[mname];
	assert(m->feature_set.size() >= 5 && m->feature_set.size() <= 5 + SYS_FEATURES);

	assert(m->feature_set.find("t._M_impl._M_start") != m->feature_set.end());
	assert(m->feature_set.find("t._M_impl._M_finish") != m->feature_set.end());
	assert(m->feature_set.find("t._M_impl._M_end_of_storage") != m->feature_set.end());
	assert(m->feature_set.find("t.artificial.size") != m->feature_set.end());
	assert(m->feature_set.find("global_feature") != m->feature_set.end());

	// Check the actual log
	assert(m->data.size() == 1);
	
	measure * mm = m->data[0];
	assert(mm->features_map.size() >= 5 && mm->features_map.size() <= 5 + SYS_FEATURES);

	assert(mm->get_feature_value("t.artificial.size") == 22);
	assert(mm->get_feature_value("global_feature") == 55);
		
	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}

bool check_Z20test_linear_branchesiii(std::map<std::string, method *> &data) {
	// Data exists
	const std::string mname = "_Z20test_linear_branchesiii";

	assert(data.find(mname) != data.end());

	// Correct and complete features
	method * m = data[mname];
	assert(m->feature_set.size() >= 4 && m->feature_set.size() <= 4 + SYS_FEATURES);

	assert(m->feature_set.find("a") != m->feature_set.end());
	assert(m->feature_set.find("global_feature") != m->feature_set.end());
	assert(m->feature_set.find("b") != m->feature_set.end());
	assert(m->feature_set.find("c") != m->feature_set.end());

	// Check the actual log
	assert(m->data.size() == 1);
	
	measure * mm = m->data[0];
	assert(mm->features_map.size() >= 4 && mm->features_map.size() <= 4 + SYS_FEATURES);

	assert(mm->get_feature_value("a") == 13);		
	assert(mm->get_feature_value("b") == 14);		
	assert(mm->get_feature_value("c") == 15);
	assert(mm->get_feature_value("global_feature") == 55);
		
	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}

bool check_Z14test_namespacePN8whatever25namespaced_abstract_classE(std::map<std::string, method *> &data) {
	// Data exists
	const std::string mname = "_Z14test_namespacePN8whatever25namespaced_abstract_classE";

	assert(data.find(mname) != data.end());

	// Correct and complete features
	method * m = data[mname];
	assert(m->feature_set.size() >= 2 && m->feature_set.size() <= 2 + SYS_FEATURES);

	// FIXME: we are borrowing the reader from the "statistics" package,
	// and there we decided not to consider _vptr as an actual feature for regressions.
	// Still, the feature is there indeed.
	//assert(m->feature_set.find("nc_vptr.namespaced_abstract_class") != m->feature_set.end());
	assert(m->feature_set.find("nc.namespaced_value") != m->feature_set.end());
	assert(m->feature_set.find("global_feature") != m->feature_set.end());

	// Check the actual log
	assert(m->data.size() == 1);
	
	measure * mm = m->data[0];
	assert(mm->features_map.size() >= 3 && mm->features_map.size() <= 3 + SYS_FEATURES);
	assert(mm->get_feature_value("nc.namespaced_value") == 5);
	assert(mm->get_feature_value("global_feature") == 55);
		
	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}

bool check_Z19test_linear_classesP11basic_class(std::map<std::string, method *> &data) {
	// Data exists
	const std::string mname = "_Z19test_linear_classesP11basic_class";

	assert(data.find(mname) != data.end());

	// Correct and complete features
	method * m = data[mname];
	assert(m->feature_set.size() >= 8 && m->feature_set.size() <= 8 + SYS_FEATURES);

	assert(m->feature_set.find("bc.private_field") != m->feature_set.end());
	assert(m->feature_set.find("bc.other_private_field") != m->feature_set.end());
	assert(m->feature_set.find("bc.fir.single_int") != m->feature_set.end());
	assert(m->feature_set.find("bc.fir.another_int") != m->feature_set.end());
	assert(m->feature_set.find("bc.arr") != m->feature_set.end());
	assert(m->feature_set.find("bc.artificial.arr_aggr") != m->feature_set.end());
	assert(m->feature_set.find("bc.useless") != m->feature_set.end());
	assert(m->feature_set.find("global_feature") != m->feature_set.end());

	// Check the actual log
	assert(m->data.size() == 1);
	
	measure * mm = m->data[0];
	assert(mm->features_map.size() >= 8 && mm->features_map.size() <= 8 + SYS_FEATURES);

	assert(mm->get_feature_value("bc.private_field") == 9);		
	assert(mm->get_feature_value("bc.other_private_field") == 11);		
	assert(mm->get_feature_value("bc.fir.single_int") == 12);
	assert(mm->get_feature_value("bc.fir.another_int") == 21);
	assert(mm->get_feature_value("bc.arr") == 20);
	assert(mm->get_feature_value("bc.artificial.arr_aggr") == 39740);
	assert(mm->get_feature_value("bc.useless") == 10);
	assert(mm->get_feature_value("global_feature") == 55);
		
	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}

bool check_Z25test_linear_fitinregister15fit_in_register(std::map<std::string, method *> &data) {
	// Data exists
	const std::string mname = "_Z25test_linear_fitinregister15fit_in_register";

	assert(data.find(mname) != data.end());

	// Correct and complete features
	method * m = data[mname];
	assert(m->feature_set.size() >= 3 && m->feature_set.size() <= 3 + SYS_FEATURES);

	assert(m->feature_set.find("fir.single_int") != m->feature_set.end());
	assert(m->feature_set.find("fir.another_int") != m->feature_set.end());
	assert(m->feature_set.find("global_feature") != m->feature_set.end());

	// Check the actual log
	assert(m->data.size() == 1);
	
	measure * mm = m->data[0];
	assert(mm->features_map.size() >= 3 && mm->features_map.size() <= 3 + SYS_FEATURES);

	assert(mm->get_feature_value("fir.single_int") == 12);		
	assert(mm->get_feature_value("fir.another_int") == 21);		
	assert(mm->get_feature_value("global_feature") == 55);
		
	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}

bool check_Z19test_linear_structsP15basic_structure(std::map<std::string, method *> &data) {
	// Data exists
	const std::string mname = "_Z19test_linear_structsP15basic_structure";

	assert(data.find(mname) != data.end());

	// Correct and complete features
	method * m = data[mname];
	assert(m->feature_set.size() >= 3 && m->feature_set.size() <= 3 + SYS_FEATURES);

	assert(m->feature_set.find("bs.useless") != m->feature_set.end());
	assert(m->feature_set.find("bs.useful") != m->feature_set.end());
	assert(m->feature_set.find("global_feature") != m->feature_set.end());

	// Check the actual log
	assert(m->data.size() == 1);
	
	measure * mm = m->data[0];
	assert(mm->features_map.size() >= 3 && mm->features_map.size() <= 3 + SYS_FEATURES);

	assert(mm->get_feature_value("bs.useless") == 7);		
	assert(mm->get_feature_value("bs.useful") == 8);		
	assert(mm->get_feature_value("global_feature") == 55);
		
	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}

bool check_Z19test_linear_charptrPc(std::map<std::string, method *> &data) {
	// Data exists
	const std::string mname = "_Z19test_linear_charptrPc";

	assert(data.find(mname) != data.end());

	// Correct and complete features
	method * m = data[mname];
	assert(m->feature_set.size() >= 2 && m->feature_set.size() <= 2 + SYS_FEATURES);

	assert(m->feature_set.find("str") != m->feature_set.end());
	assert(m->feature_set.find("global_feature") != m->feature_set.end());

	// Check the actual log
	assert(m->data.size() == 1);
	
	measure * mm = m->data[0];
	assert(mm->features_map.size() >= 2 && mm->features_map.size() <= 2 + SYS_FEATURES);

	assert(mm->get_feature_value("str") == 6);		
	assert(mm->get_feature_value("global_feature") == 55);
		
	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}

bool check_Z16test_quad_int_wnii(std::map<std::string, method *> &data) {
	// Data exists
	const std::string mname = "_Z16test_quad_int_wnii";

	assert(data.find(mname) != data.end());

	// Correct and complete features
	method * m = data[mname];
	assert(m->feature_set.size() >= 3 && m->feature_set.size() <= 3 + SYS_FEATURES);

	assert(m->feature_set.find("t") != m->feature_set.end());
	assert(m->feature_set.find("nlevel") != m->feature_set.end());
	assert(m->feature_set.find("global_feature") != m->feature_set.end());

	// Check the actual log
	assert(m->data.size() == 1);
	
	measure * mm = m->data[0];
	assert(mm->features_map.size() >= 3 && mm->features_map.size() <= 3 + SYS_FEATURES);

	assert(mm->get_feature_value("t") == 4);		
	assert(mm->get_feature_value("nlevel") == 5);
	assert(mm->get_feature_value("global_feature") == 55);
		
	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}

bool check_Z13test_quad_inti(std::map<std::string, method *> &data) {
	// Data exists
	const std::string mname = "_Z13test_quad_inti";

	assert(data.find(mname) != data.end());

	// Correct and complete features
	method * m = data[mname];
	assert(m->feature_set.size() >= 2 && m->feature_set.size() <= 2 + SYS_FEATURES);

	assert(m->feature_set.find("t") != m->feature_set.end());
	assert(m->feature_set.find("global_feature") != m->feature_set.end());

	// Check the actual log
	assert(m->data.size() == 1);
	
	measure * mm = m->data[0];
	assert(mm->features_map.size() >= 2 && mm->features_map.size() <= 2 + SYS_FEATURES);

	assert(mm->get_feature_value("t") == 3);		
	assert(mm->get_feature_value("global_feature") == 55);
		
	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}

bool check_Z23test_linear_int_pointerPi(std::map<std::string, method *> &data) {
	// Data exists
	const std::string mname = "_Z23test_linear_int_pointerPi";

	assert(data.find(mname) != data.end());

	// Correct and complete features
	method * m = data[mname];
	assert(m->feature_set.size() >= 2 && m->feature_set.size() <= 2 + SYS_FEATURES);

	assert(m->feature_set.find("t") != m->feature_set.end());
	assert(m->feature_set.find("global_feature") != m->feature_set.end());

	// Check the actual log
	assert(m->data.size() == 1);
	
	measure * mm = m->data[0];
	assert(mm->features_map.size() >= 2 && mm->features_map.size() <= 2 + SYS_FEATURES);

	assert(mm->get_feature_value("t") == 111);		
	assert(mm->get_feature_value("global_feature") == 55);
		
	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}

bool check_Z15test_linear_inti(std::map<std::string, method *> &data) {
	// Data exists
	const std::string mname = "_Z15test_linear_inti";

	assert(data.find(mname) != data.end());

	// Correct and complete features
	method * m = data[mname];
	assert(m->feature_set.size() >= 2 && m->feature_set.size() <= 2 + SYS_FEATURES);

	assert(m->feature_set.find("t") != m->feature_set.end());
	assert(m->feature_set.find("global_feature") != m->feature_set.end());

	// Check the actual log
	assert(m->data.size() == 1);
	
	measure * mm = m->data[0];
	assert(mm->features_map.size() >= 2 && mm->features_map.size() <= 2 + SYS_FEATURES);

	assert(mm->get_feature_value("t") == 1);		
	assert(mm->get_feature_value("global_feature") == 55);
		
	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}

bool check_Z17test_linear_floatf(std::map<std::string, method *> &data) {
	// Data exists
	const std::string mname = "_Z17test_linear_floatf";

	assert(data.find(mname) != data.end());

	// Correct and complete features
	method * m = data[mname];
	assert(m->feature_set.size() >= 2 && m->feature_set.size() <= 2 + SYS_FEATURES);

	assert(m->feature_set.find("t") != m->feature_set.end());
	assert(m->feature_set.find("global_feature") != m->feature_set.end());

	// Check the actual log
	assert(m->data.size() == 1);

	measure * mm = m->data[0];
	assert(mm->features_map.size() >= 2 && mm->features_map.size() <= 2 + SYS_FEATURES);

	assert(mm->get_feature_value("t") == 2);
	assert(mm->get_feature_value("global_feature") == 55);

	std::cout << "\033[1;32m Success! \033[0m" << std::endl;
	return false;
}

