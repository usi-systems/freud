#include "table-generator.hh"

extern std::unordered_map<std::string, std::list<std::string>> artificial_features;

/// ************ PRIVATE ************ ////
bool table_generator::check_features(dwarf_explorer * de, const std::string sym_name, const std::string type_name) {
	if (de->type_is_undefined(sym_name))
		return false;

	// --- SWITCH: complex vs basic type
	if (de->hierarchy_tree_nodes_map.find(sym_name) != de->hierarchy_tree_nodes_map.end()) {
		// --- SWITCH: complex type
		// even if polymorphic, there should be at least the vptr
		// so, no need to explore the hierarchy tree
		return !de->types.at(type_name).empty();  
	} else if (basic_features.find(type_name) != basic_features.end()) {
		// --- SWITCH: primitive type
		return (basic_features.at(type_name).num_of_values > 0);
	} else {
		utils::log(VL_ERROR, "Could not find type " + type_name);
		return false;
	}
}

void table_generator::create_table_entries_for_classes_dfs(dwarf_explorer * de, int tree_size, std::string &descriptors_code, const std::string sym_name, const std::string ppname) {
	struct hierarchy_tree_node * htn = de->hierarchy_tree_nodes_map.at(sym_name);
	descriptors_code += get_table_entry_for_node(de, tree_size, htn, ppname);

	// GO DFS
	for (hierarchy_tree_node * htn_c: htn->maybe_real_type) {
		create_table_entries_for_classes_dfs(de, tree_size, descriptors_code, htn_c->class_name, ppname);
	}
}

std::string table_generator::get_table_entry_for_node(dwarf_explorer * de, int tree_size, const struct hierarchy_tree_node * mrt, const std::string ppname) {
	std::string descriptors_code;
	// HASH OF THE RUNTIME OBJECT TYPE
	if (tree_size > 1)
		descriptors_code = mrt->sym_lname_hash + "\n";
	else
		descriptors_code = mrt->sym_name_hash + "\n";

	std::string pp_type_name = "class " + mrt->class_name;
	if (de->types.find(pp_type_name) == de->types.end()) {
		pp_type_name = "struct " + mrt->class_name;
		if (de->types.find(pp_type_name) == de->types.end()) {
			utils::log(VL_DEBUG, "Need to find definition for " + pp_type_name);
			std::vector<uint32_t> empty_vec;
			std::unordered_set<std::string> empty_uset;
			std::list<struct member> members;
			std::string name = "";
			bool false_bool = false;
			struct array_info empty_ai;
			std::string empty_str = "";
			unsigned int num_features = 0;
			std::string complex_type;
			bool found = de->find_definition(mrt->class_name, empty_vec, empty_uset, members, empty_vec, name, false_bool, empty_ai, false_bool, empty_str, num_features, complex_type);
			if (found) {
				pp_type_name = complex_type + mrt->class_name;
				de->types[pp_type_name] = members;
			} else {
				utils::log(VL_ERROR, "Could not find definition for " + pp_type_name);
				descriptors_code += std::to_string(0) + "\n"; 		
				return descriptors_code;
			}
		}
	}

	unsigned int num_of_features = 0;
	std::string member_names = "";
	bool first = false, last = false, size_found = false;	
	struct member first_m, last_m;
	for (struct member m: de->types[pp_type_name]) {
		if (basic_features.find(m.type_name) != basic_features.end()) {
			if (basic_features.at(m.type_name).num_of_values == 0) {
				utils::log(VL_ERROR, "Missing basic type " + m.type_name);
				exit(-1);
			}
			num_of_features += basic_features.at(m.type_name).num_of_values;
			for (std::string n: m.feat_names) {
				member_names += m.type_name + "\n";
				member_names += ppname + n + "\n";
			}
			if (m.feat_names.size() != basic_features.at(m.type_name).num_of_values) {
				utils::log(VL_ERROR, "Inconsistent data! T: " + pp_type_name + " - " + m.feat_names.front() + "; " + std::to_string(m.feat_names.size()) + " - " + std::to_string(basic_features.at(m.type_name).num_of_values));
				utils::log(VL_ERROR, m.type_name + " should have " + std::to_string(basic_features.at(m.type_name).num_of_values) + " values, not " + std::to_string(m.feat_names.size()));
				for (std::string fname: m.feat_names) {
					utils::log(VL_ERROR, m.type_name + " " + fname);
				}
				exit(-1);
			}
		} else {
			utils::log(VL_ERROR, "Missing basic type " + m.type_name);
			exit(-1);
		}

		/*
		 *	HEURISTIC, PART1: LOOK FOR PAIRS LIKE (START,END), (FIRST,LAST)... 
		 */
		std::string uname = utils::uppercase(m.feat_names.front()); 
		if (
				uname.find("FIRST") != std::string::npos ||
				uname.find("START") != std::string::npos ||
				uname.find("BEGIN") != std::string::npos 
				) {
			if (first) {
				utils::log(VL_DEBUG, ppname + ": there's more than one possible first member for heuristic 1, skipping the last");
			} else {
				first = true;
				first_m = m;
			}
		}
		else if (
				uname.find("LAST") != std::string::npos ||
				uname.find("STOP") != std::string::npos ||
				uname.find("FINISH") != std::string::npos ||
				uname.find("END") != std::string::npos 
				) {
			if (last) {
				utils::log(VL_DEBUG, ppname + ": there's more than one possible last member for heuristic 1, skipping the last");
			} else {
				last = true;
				last_m = m;
			}
		}
	}
	if (first && last && first_m.type_name == last_m.type_name) {
		size_found = true;
		// TODO: remove this hack
		// right now I know I have only one artificial feature, but not necessarily true
		if (artificial_features.empty())
			artificial_features[pp_type_name].push_back("size");
	}
	if (artificial_features.find(pp_type_name) != artificial_features.end()) {
		num_of_features += artificial_features[pp_type_name].size();
		for (std::string af_name: artificial_features[pp_type_name]) {
			member_names += "unsigned int\n"; // right now it's only size, so unsigned int should be good
			member_names += ppname + ".artificial." + af_name + "\n";
		}
	}

	// NUM_OF_FEATURES
	descriptors_code += std::to_string(num_of_features) + "\n";
	// FEATURE NAMES
	descriptors_code += member_names;
	return descriptors_code;
}


/// ************ PUBLIC ************ ////
void table_generator::create_table(dwarf_explorer * de, std::string tbl_filename) {
	std::ofstream descf(tbl_filename);
	std::string descriptors_code = "";

	// FOR EACH SYMBOL
	for (auto p: de->func_pars_map) {
		if (p.second.size() == 0)
			continue; // no features, skip

		descriptors_code += "###\n";
		// SYMBOL NAME
		descriptors_code += p.first + "\n";
		if (de->func_addr_map.find(p.first) == de->func_addr_map.end()) {
			utils::log(VL_ERROR, "Couldn't find entry address for " + p.first);
			exit(-1);
		}
		
		// INSTRUMENTATION ENTRY POINT
		descriptors_code += std::to_string(de->func_addr_map[p.first]) + "\n";

		size_t sz = p.second.size();
		std::unordered_set<std::string> to_skip;

		for (auto pp: p.second) {
			std::string sym_name = pp.type_name.substr(pp.type_name.find(" ") + 1, std::string::npos);

			// Check whether it actually has features
			if (check_features(de, sym_name, pp.type_name) == false) {
				to_skip.insert(sym_name);
				sz--;
			}
		}
		
		// NUM OF FORMAL PARAMETERS
		descriptors_code += std::to_string(sz) + "\n";
		for (auto pp: p.second) {
			std::string sym_name = pp.type_name.substr(pp.type_name.find(" ") + 1, std::string::npos);
			if (to_skip.find(sym_name) == to_skip.end()) { //!de->type_is_undefined(sym_name)) {
				/*****
				 * DESCRIPTOR INFO  
				****/

				// LOCATION
				descriptors_code += std::to_string(pp.location) + "\n";

				// OFFSET FROM LOCATION 
				descriptors_code += std::to_string(pp.offset_from_location) + "\n";
				
				// IS_ADDR or REG. CONTAINING ADDR
				descriptors_code += std::to_string(pp.is_addr + pp.is_register_with_addr) + "\n";
				
				// IS_PTR; THE PARAMETER REPRESENTS A PTR
				descriptors_code += std::to_string(pp.is_ptr) + "\n";

				// NAME OF THE TYPE OF THE FORMAL PARAMETER
				descriptors_code += pp.type_name + "\n";
				
				// --- SWITCH: complex vs basic type
				if (de->hierarchy_tree_nodes_map.find(sym_name) != de->hierarchy_tree_nodes_map.end()) {
					// --- SWITCH: complex type

					// NUMBER OF POTENTIAL RUNTIME TYPES (for polymorphic types)
					int tree_size = de->get_class_graph_size(sym_name);
					descriptors_code += std::to_string(tree_size) + "\n";

					create_table_entries_for_classes_dfs(de, tree_size, descriptors_code, sym_name, pp.name);
				} else if (basic_features.find(pp.type_name) != basic_features.end()) {
					// --- SWITCH: primitive type
					
					// NUMBER OF POTENTIAL RUNTIME TYPES (for basic types)
					descriptors_code += std::to_string(1) + "\n";
					
					// HASH OF THE RUNTIME OBJECT TYPE (always 0)
					descriptors_code += std::to_string(0) + "\n";

					// NUM OF FEATURES
					unsigned feat_num = basic_features.at(pp.type_name).num_of_values;
					descriptors_code += std::to_string(feat_num) + "\n";

					// FEAT_NAMES
					for (int f = 0; f < feat_num; f++) {
						descriptors_code += pp.type_name + "\n";
						descriptors_code += pp.name + (f > 0 ? std::to_string(f) : "") + "\n";
					}
					// TODO: handle arrays decently
					// Arrays have 2 values, where the second is an aggregate function of the values in the array 
					//if (pp.type_name.find("array ") == 0) 
					//	descriptors_code += "artificial_" + pp.name + "_aggr\n"; 
				} else {
					utils::log(VL_ERROR, "Could not find type " + pp.type_name);
					// JUST SAY 0 RUNTIME TYPES
					descriptors_code += std::to_string(0) + "\n";
				}
			} else {
				utils::log(VL_ERROR, "Could not find type " + sym_name);
			}
			// ##########################################################
		}
	}
	descf << descriptors_code;
	descf.close();
}

