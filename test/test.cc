/*
Copyright 2020 Daniele Rogora

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "../freud-statistics/method.hh"
#include "../freud-statistics/reader.hh"
#include "../freud-statistics/regression.hh"
#include "../freud-statistics/reader_annotations.hh"

#include <assert.h>

#include "instr_tests.hh"

#define TEST_STATS

#ifdef TEST_STATS
#include "stats_tests.hh"
#endif

// enums defined in analysis/const.hh
/*
FT_BOOL = 0,
FT_INT = 1,
FT_COLLECTION = 2,
FT_COLLECTION_AGGR = 3,
FT_STRING = 4, // and also mysqllexstring are caught here
FT_PATH = 5,
FT_NASCII = 6,
FT_SIZE = 7,
FT_RESOURCE = 8,
FT_BRANCH_EXEC = 9,
FT_BRANCH_ISTRUE = 10,
FT_STRUCT_MEMBER = 11,
FT_STRUCT_MEMBER_2 = 12,
...
FT_STRUCT_MEMBER_899 = 910,
*/

bool test_instrumentation_output() {
	bool err = false;
	std::unordered_map<uint64_t, measure *> m_ids_map;
	std::map<std::string, method *> data;
	
	// _Z15test_linear_inti
	std::cout << "Checking _Z15test_linear_inti" << std::endl;
	data.clear();
	m_ids_map.clear();
	reader::read_folder("symbols/_Z15test_linear_inti/", data, m_ids_map);
	err |= check_Z15test_linear_inti(data);

	// _Z23test_linear_int_pointerPi
	std::cout << "Checking _Z23test_linear_int_pointerPi" << std::endl;
	data.clear();
	m_ids_map.clear();
	reader::read_folder("symbols/_Z23test_linear_int_pointerPi/", data, m_ids_map);
	err |= check_Z23test_linear_int_pointerPi(data);

	// _Z17test_linear_floatf
	std::cout << "Checking _Z17test_linear_floatf" << std::endl;
	data.clear();
	m_ids_map.clear();
	reader::read_folder("symbols/_Z17test_linear_floatf/", data, m_ids_map);
	err |= check_Z17test_linear_floatf(data);

	// _Z13test_quad_inti
	std::cout << "Checking _Z13test_quad_inti" << std::endl;
	data.clear();
	m_ids_map.clear();
	reader::read_folder("symbols/_Z13test_quad_inti/", data, m_ids_map);
	err |= check_Z13test_quad_inti(data);

	// _Z16test_quad_int_wnii
	std::cout << "Checking _Z16test_quad_int_wnii" << std::endl;
	data.clear();
	m_ids_map.clear();
	reader::read_folder("symbols/_Z16test_quad_int_wnii/", data, m_ids_map);
	err |= check_Z16test_quad_int_wnii(data);

	// _Z19test_linear_charptrPc	
	std::cout << "Checking _Z19test_linear_charptrPc" << std::endl;
	data.clear();
	m_ids_map.clear();
	reader::read_folder("symbols/_Z19test_linear_charptrPc/", data, m_ids_map);
	err |= check_Z19test_linear_charptrPc(data);

	// _Z19test_linear_structsP15basic_structure	
	std::cout << "Checking _Z19test_linear_structsP15basic_structure" << std::endl;
	data.clear();
	m_ids_map.clear();
	reader::read_folder("symbols/_Z19test_linear_structsP15basic_structure/", data, m_ids_map);
	err |= check_Z19test_linear_structsP15basic_structure(data);

	// _Z19test_linear_classesP11basic_class
	std::cout << "Checking _Z19test_linear_classesP11basic_class" << std::endl;
	data.clear();
	m_ids_map.clear();
	reader::read_folder("symbols/_Z19test_linear_classesP11basic_class/", data, m_ids_map);
	err |= check_Z19test_linear_classesP11basic_class(data);

	// _Z25test_linear_fitinregister15fit_in_register
	std::cout << "Checking _Z25test_linear_fitinregister15fit_in_register" << std::endl;
	data.clear();
	m_ids_map.clear();
	reader::read_folder("symbols/_Z25test_linear_fitinregister15fit_in_register/", data, m_ids_map);
	err |= check_Z25test_linear_fitinregister15fit_in_register(data);

	// _Z20test_linear_branchesiii
	std::cout << "Checking _Z20test_linear_branchesiii" << std::endl;
	data.clear();
	m_ids_map.clear();
	reader::read_folder("symbols/_Z20test_linear_branchesiii/", data, m_ids_map);
	err |= check_Z20test_linear_branchesiii(data);

	// _Z18test_linear_vectorPSt6vectorIiSaIiEE
	std::cout << "Checking _Z18test_linear_vectorPSt6vectorIiSaIiEE" << std::endl;
	data.clear();
	m_ids_map.clear();
	reader::read_folder("symbols/_Z18test_linear_vectorPSt6vectorIiSaIiEE/", data, m_ids_map);
	err |= check_Z18test_linear_vectorPSt6vectorIiSaIiEE(data);

	// _Z18test_linear_farrayPA10_j	
	std::cout << "Checking _Z18test_linear_farrayPA10_j" << std::endl;
	data.clear();
	m_ids_map.clear();
	reader::read_folder("symbols/_Z18test_linear_farrayPA10_j/", data, m_ids_map);
	err |= check_Z18test_linear_farrayPA10_j(data);

	// _Z14test_namespacePN8whatever25namespaced_abstract_classE	
	std::cout << "Checking _Z14test_namespacePN8whatever25namespaced_abstract_classE" << std::endl;
	data.clear();
	m_ids_map.clear();
	reader::read_folder("symbols/_Z14test_namespacePN8whatever25namespaced_abstract_classE/", data, m_ids_map);
	err |= check_Z14test_namespacePN8whatever25namespaced_abstract_classE(data);

	return err;
}

#ifdef TEST_STATS
bool test_statistical_analysis() {
	bool err = false;
	std::map<std::string, annotation *> m_ann_map;
	
	// _Z15test_linear_inti
	m_ann_map.clear();
	std::string sym_name = "_Z15test_linear_inti";
	std::cout << "Checking annotation for " << sym_name << std::endl;
	reader_annotations::read_annotations_file("ann/" + sym_name + ".txt", sym_name, m_ann_map);
	err |= check_ann_Z15test_linear_inti(m_ann_map);

	// _Z23test_linear_int_pointerPi
	
	// _Z13test_quad_inti
	sym_name = "_Z13test_quad_inti";
	std::cout << "Checking " << sym_name << std::endl;
	m_ann_map.clear();
	std::cout << "Checking annotation for " << sym_name << std::endl;
	reader_annotations::read_annotations_file("ann/" + sym_name + ".txt", sym_name, m_ann_map);
	err |= check_ann_Z13test_quad_inti(m_ann_map);

	// _Z16test_quad_int_wnii
	sym_name = "_Z16test_quad_int_wnii";
	std::cout << "Checking " << sym_name << std::endl;
	m_ann_map.clear();
	std::cout << "Checking annotation for " << sym_name << std::endl;
	reader_annotations::read_annotations_file("ann/" + sym_name + ".txt", sym_name, m_ann_map);
	err |= check_ann_Z16test_quad_int_wnii(m_ann_map);

	// _Z19test_linear_charptrPc	
	sym_name = "_Z19test_linear_charptrPc";
	std::cout << "Checking " << sym_name << std::endl;
	m_ann_map.clear();
	std::cout << "Checking annotation for " << sym_name << std::endl;
	reader_annotations::read_annotations_file("ann/" + sym_name + ".txt", sym_name, m_ann_map);
	err |= check_ann_Z19test_linear_charptrPc(m_ann_map);

	// _Z19test_linear_structsP15basic_structure	
	sym_name = "_Z19test_linear_structsP15basic_structure";
	std::cout << "Checking " << sym_name << std::endl;
	m_ann_map.clear();
	std::cout << "Checking annotation for " << sym_name << std::endl;
	reader_annotations::read_annotations_file("ann/" + sym_name + ".txt", sym_name, m_ann_map);
	err |= check_ann_Z19test_linear_structsP15basic_structure(m_ann_map);

	// _Z19test_linear_classesP11basic_class	
	sym_name = "_Z19test_linear_classesP11basic_class";
	std::cout << "Checking " << sym_name << std::endl;
	m_ann_map.clear();
	std::cout << "Checking annotation for " << sym_name << std::endl;
	reader_annotations::read_annotations_file("ann/" + sym_name + ".txt", sym_name, m_ann_map);
	err |= check_ann_Z19test_linear_classesP11basic_class(m_ann_map);

	// _Z25test_linear_fitinregister15fit_in_register
	sym_name = "_Z25test_linear_fitinregister15fit_in_register";
	std::cout << "Checking " << sym_name << std::endl;
	m_ann_map.clear();
	std::cout << "Checking annotation for " << sym_name << std::endl;
	reader_annotations::read_annotations_file("ann/" + sym_name + ".txt", sym_name, m_ann_map);
	err |= check_ann_Z25test_linear_fitinregister15fit_in_register(m_ann_map);

	// _Z20test_linear_branchesiii
	sym_name = "_Z20test_linear_branchesiii";
	std::cout << "Checking " << sym_name << std::endl;
	m_ann_map.clear();
	std::cout << "Checking annotation for " << sym_name << std::endl;
	reader_annotations::read_annotations_file("ann/" + sym_name + ".txt", sym_name, m_ann_map);
	err |= check_ann_Z20test_linear_branchesiii(m_ann_map);

	// _Z18test_linear_vectorPSt6vectorIiSaIiEE
	sym_name = "_Z18test_linear_vectorPSt6vectorIiSaIiEE";
	std::cout << "Checking " << sym_name << std::endl;
	m_ann_map.clear();
	std::cout << "Checking annotation for " << sym_name << std::endl;
	reader_annotations::read_annotations_file("ann/" + sym_name + ".txt", sym_name, m_ann_map);
	err |= check_ann_Z18test_linear_vectorPSt6vectorIiSaIiEE(m_ann_map);
	
	// _Z18test_linear_farrayPA10_j	
	sym_name = "_Z18test_linear_farrayPA10_j";
	std::cout << "Checking " << sym_name << std::endl;
	m_ann_map.clear();
	std::cout << "Checking annotation for " << sym_name << std::endl;
	reader_annotations::read_annotations_file("ann/" + sym_name + ".txt", sym_name, m_ann_map);
	err |= check_ann_Z18test_linear_farrayPA10_j(m_ann_map);

	// _Z22test_random_clusteringi
	sym_name = "_Z22test_random_clusteringi";
	std::cout << "Checking " << sym_name << std::endl;
	m_ann_map.clear();
	std::cout << "Checking annotation for " << sym_name << std::endl;
	reader_annotations::read_annotations_file("ann/" + sym_name + ".txt", sym_name, m_ann_map);
	err |= check_ann_Z22test_random_clusteringi(m_ann_map);

	
	return false;
}
#endif

int main(int argc, char *argv[]) {

	if (argc <= 1) {
		std::cout << "you must specify either --test-instr or --test-stats" << std::endl;
		return 0;
	}

	if (strcmp(argv[1], "--test-instr") == 0) {
		std::cout << "\033[1;34m ### TESTING INSTRUMENTATION ### \033[0m" << std::endl;
		if (test_instrumentation_output()) {
			std::cout << "\033[1;31m Could not complete all tests successfully! \033[0m" << std::endl;
			return -1;
		}
		std::cout << "\033[1;32m All tests completed successfully! \033[0m" << std::endl;
	}
#ifdef TEST_STATS
	else if (strcmp(argv[1], "--test-stats") == 0) {
		std::cout << "\033[1;34m ### TESTING STATISTICS ### \033[0m" << std::endl;
		if (test_statistical_analysis()) {
			std::cout << "\033[1;31m Could not complete all tests successfully! \033[0m" << std::endl;
			return -1;
		}
		std::cout << "\033[1;32m All tests completed successfully! \033[0m" << std::endl;
	}
#endif
	else {
		std::cout << "Could not parse parameter, or feature not enabled: " << argv[1] << std::endl;
		return -1;
	}
	
}
