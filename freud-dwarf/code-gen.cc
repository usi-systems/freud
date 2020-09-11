#include <fstream>

#include "code-gen.hh"
#include "utils.hh"

extern std::unordered_map<std::string, std::list<std::string>> artificial_features;

std::string code_generator::get_complex_feature_processing_text_from_addr(const struct member &m, std::unordered_set<std::string> &used_names) {
	// THIS METHOD IS CALLED ONLY FOR NON-BASIC FEATURES
	utils::log(VL_DEBUG, "GFPCFA | " + m.feat_names.front() + " - " + std::to_string(m.pointer.size()) + " - " + std::to_string(m.offset.size()));
	
	// Make sure we are not using the same name two times in the same scope
	std::string unique_name = m.feat_names.front();
	while (used_names.find(utils::copy_validate_f_name(unique_name)) != used_names.end()) {
		unique_name += "2";
	}
	used_names.insert(utils::copy_validate_f_name(unique_name));
	
	std::string feature_processing;
	std::string tn = m.type_name;
	if (tn.find("enum ") == 0)
		tn = tn.substr(strlen("enum "), std::string::npos);
	else if (tn.find("array ") == 0) 
		tn = tn.substr(strlen("array "), std::string::npos);
	
	if (basic_features.find(m.type_name) != basic_features.end()) {
		std::string front_part = "";
		std::string back_part = "";
		bool assigned = false, used_pinsafecopy = false;
		unsigned int reffed = 0;
		for (unsigned int p = 0; p < m.pointer.size() - 1; p++) {
			unsigned int deref_level = m.pointer[p + 1];
			// If last level, check whether it's an array
			if (p == m.pointer.size() - 2)
				// In which case, remove 1 dereferencing level
				deref_level -= (m.ai.dims > 0);
			if (deref_level == 0) {
				if (p > 0 && m.offset[p] > 0)
					front_part += "foo += " + std::to_string(m.offset[p]) + ";";
			} else {
				for (unsigned int i = 0; i < deref_level; i++) {
					assigned = true;
					front_part += "if (PIN_SafeCopy(&foo, (void *)(foo + " + std::to_string(m.offset[p]) + "), sizeof(ADDRINT)) == sizeof(ADDRINT)) {";
					used_pinsafecopy = true;
					if (p == m.pointer.size() - 2) // I'm interested in the dereferencing of the last level, only
						reffed++;
				}
				for (unsigned int i = 0; i < deref_level; i++) {
					back_part += "}";
				}
			}
		}
		if (!assigned) {
			feature_processing += "foo = (struct_base_address + " + std::to_string(m.offset[0]) + ");";
		} else {
			feature_processing += "foo = struct_base_address;";
		}
		unsigned int last_pointer = m.pointer[m.pointer.size() - 1];
		feature_processing +=  tn;

		while (last_pointer + 1 - (int)(reffed + (m.ai.dims > 0)) > 0) {
			feature_processing += "*";
			last_pointer--;
		}
		feature_processing += " " + utils::copy_validate_f_name(unique_name) + " = 0;";
		feature_processing += front_part; 
		
		feature_processing += utils::copy_validate_f_name(unique_name) + " = " + "(" + tn;
		
		// I know the size is at least 1, because we're processing a member of a structure
		last_pointer = m.pointer[m.pointer.size() - 1];
		while (last_pointer + 1 - (reffed + (m.ai.dims > 0)) > 0) {
			feature_processing += "*";
			last_pointer--;
		}
		if (m.ai.dims == 0) // not an array with fixed size
			feature_processing += ")(foo);" + back_part + "re->add_feature_value(" + utils::copy_validate_f_name(unique_name) + ");\n";
		else {
			utils::log(VL_DEBUG, "Found an array! Dims: " + m.ai.dims);
			feature_processing += ")(foo);";
#ifdef USE_VECTORS_FOR_VARIADIC
			feature_processing += "counts.clear();";
			for (unsigned int d: m.ai.counts) {
				feature_processing += "counts.push_back(" + std::to_string(d) + ");";
			} 
			feature_processing += "re->add_feature_value_array(" + utils::copy_validate_f_name(unique_name) + ", (unsigned int)" + std::to_string(m.ai.dims) + ", &counts);\n" + back_part;
#else
			feature_processing += "re->add_feature_value_array_var(" + utils::copy_validate_f_name(unique_name) + ", (unsigned int)" + std::to_string(m.ai.dims);
			for (unsigned int d: m.ai.counts) {
				feature_processing += ", " + std::to_string(d);
			}
			feature_processing += ");\n" + back_part; 
#endif
			if (used_pinsafecopy) // this is needed only if we copied stuff with PIN_SafeCopy
				feature_processing += "else { re->add_feature_value((int)0); re->add_feature_value((int)0); }\n"; // placeholder to keep intact the indices of features...
		}
	}
	return feature_processing;
}

std::string code_generator::get_complex_feature_processing_text_from_reg(const struct member &m, std::unordered_set<std::string> &used_names, bool &can_add_size) {
	// THIS METHOD IS CALLED ONLY FOR NON-BASIC FEATURES
	utils::log(VL_DEBUG, "GFPCFR | " + m.feat_names.front() + " - " + std::to_string(m.pointer.size()) + " - " + std::to_string(m.offset.size()));
	can_add_size = true;
	// Make sure we are not using the same name two times in the same scope
	std::string unique_name = m.feat_names.front() + "R";
	while (used_names.find(utils::copy_validate_f_name(unique_name)) != used_names.end()) {
		unique_name += "2";
	}
	used_names.insert(utils::copy_validate_f_name(unique_name));
	
	
	// NOT A PTR; Does it fit in one register?
	// TODO: fix this thing, check where a big structure goes, if passed through registers... 
	std::string feature_processing;
	std::string tn = m.type_name;
	if (tn.find("enum ") == 0)
		tn = tn.substr(strlen("enum "), std::string::npos);
	if (tn.find("array ") == 0)
		tn = tn.substr(strlen("array "), std::string::npos);
	if (basic_features.find(m.type_name) != basic_features.end()) {
		// Any other offset greater than 4 should not make sense without a pointer
		if ((m.offset[0] / 8) <= 6) { // Ignore things that clearly wouldn't fit
			feature_processing +=  tn;
			unsigned int last_pointer = m.pointer[m.pointer.size() - 1];
			while (last_pointer > 0) {
				feature_processing += "*";
				last_pointer--;
			}
			feature_processing +=  " " + utils::copy_validate_f_name(unique_name) + " = (" + tn + " ";
			last_pointer = m.pointer[m.pointer.size() - 1];
			while (last_pointer > 0) {
				feature_processing += "*";
				last_pointer--;
			}
			feature_processing += ")";
			if (m.offset[0] % 8 == 0) {
					feature_processing += " (input_args[desc->params[q].position + " + std::to_string(m.offset[0] / 8)  + "]);";
			}
			else { // not a ptr, but I have an offset of 4 bytes; this is stored in the same 8 byte register
				// TODO: does this thing apply also when the variable is in the stack, and not in register?
				feature_processing += " (input_args[desc->params[q].position + " + std::to_string(m.offset[0] / 8)  + "] >> " + std::to_string((m.offset[0] % 8) * 8) + ");"; 
			}
			feature_processing += " re->add_feature_value(" + utils::copy_validate_f_name(unique_name) + ");\n";
		} else {
			can_add_size = false;
		}
	}
	return feature_processing;
}

void code_generator::create_instrumentation_code(const dwarf_explorer * de, std::string fprocessfname) {
	std::ofstream fprocess(fprocessfname);
	std::string feature_processing = "extern uint64_t base_address;\n";
#ifdef USE_VECTORS_FOR_VARIADIC
	feature_processing += "vector<unsigned int> counts;\n";
#endif
	std::string signatures = "extern \"C\" {\n";
	std::unordered_set<std::string> used_names;

	feature_processing += "uint64_t static_type_hash = freud_hash((unsigned char *)desc->params[q].type_name.c_str());\n";
	feature_processing += "switch (static_type_hash) {\n";
	// Add the type-specific code that extracts features from data types, basic or not	
	for (std::pair<std::string, std::list<struct member>> type_pair: de->types) {
		used_names.clear();
		std::string t = type_pair.first;

		if (basic_features.find(t) != basic_features.end()) {
			signatures += "// " + t + "\n";
			signatures += utils::get_signature(t);
		}

		std::string sn = t.substr(t.find(" ") + 1, std::string::npos);
		if (de->type_is_undefined(sn))
			continue;
		if (basic_features.find(t) == basic_features.end()
			&& type_pair.second.empty()) {
			utils::log(VL_DEBUG, "Warning: no members for " + type_pair.first);
			continue;
		}

		feature_processing += "// STYPE: " + t + "\n";
		feature_processing += "case " + std::to_string(utils::hash((unsigned char *)t.c_str())) + "lu: {\n";
		
		/**
		 *	BASIC FEATURE
		 *  char, int, long, float, double...
		 */
		if (basic_features.find(t) != basic_features.end()) {

			// Remove "enum" from the data type, if present
			std::string tn = t;
			bool is_array = false;
			if (tn.find("enum ") == 0)
				tn = tn.substr(strlen("enum "), std::string::npos);
			else if (tn.find("array ") == 0) {
				tn = tn.substr(strlen("array "), std::string::npos);
				is_array = true;
			}
			if (is_array == false) {
				// *** Option 1: THE LOCATION IS A REGISTER ***
				feature_processing += "if (desc->params[q].is_addr == false) {\n";  	
				
				// If the basic feature is a ptr, just read the address from the register
				feature_processing +=  "if (desc->params[q].is_ptr) { re->add_feature_value((" + tn + " *)(input_args[desc->params[q].position] + desc->params[q].offset));}\n";
				
				// Else, the actual value is already stored in the register	
				feature_processing +=  "else { re->add_feature_value((" + tn + ")input_args[desc->params[q].position";
				// If the basic feature is a floating pt, then it's stored in XMM registers 
				if (t == "float" || t == "double") {
					// These should be float arguments... 
					feature_processing +=  " - 6";
				}			
				feature_processing += "] + desc->params[q].offset);}\n";

				feature_processing += "}\nelse {\n"; // else is_addr
					
					// *** Option 2: THE LOCATION IS AN ADDRESS ***
					feature_processing += "if (desc->params[q].is_reg_with_addr == false) {\n";  	
						// It's in a given address
						// Here it should not matter whether it is int or float
						feature_processing +=  "if (desc->params[q].is_ptr) { re->add_feature_value((" + tn + " **)(desc->params[q].position + desc->params[q].offset + base_address));}\n";
						feature_processing +=  "else { re->add_feature_value((" + tn + "*)(desc->params[q].position + desc->params[q].offset + base_address));}\n";
				
					// *** Option 3: THE LOCATION IS AN OFFSET FROM THE VALUE OF A REGISTER ***
					feature_processing +=  "} else {\n";
						feature_processing +=  "if (desc->params[q].is_ptr) { re->add_feature_value((" + tn + " **)(input_args[desc->params[q].position] + desc->params[q].offset));}\n";
						feature_processing +=  "else { re->add_feature_value((" + tn + "*)(input_args[desc->params[q].position] + desc->params[q].offset));}\n";
					feature_processing +=  "}\n";
				feature_processing +=  "}\n";
			} 
			else {
				// THIS THING IS AN ARRAY!
				// *** Option 1: THE LOCATION IS A REGISTER ***
				feature_processing += "if (desc->params[q].is_addr == false) {\n";  	
				
				// This is an array, so it is a ptr, just read the address from the register
				feature_processing +=  "re->add_feature_value_array((" + tn + " *)(input_args[desc->params[q].position] + desc->params[q].offset), desc->params[q].array_dims, &desc->params[q].array_counts);\n";
				
				feature_processing += "}\nelse {\n"; // else is_addr
					// *** Option 2: THE LOCATION IS AN ADDRESS ***
					feature_processing += tn + " * arr_addr = 0;";	
					feature_processing += "if (desc->params[q].is_reg_with_addr == false) {\n";  	
						// It's in a given address
						// Here it should not matter whether it is int or float
						// Make sure the address with the address is readable
						feature_processing += "if (desc->params[q].is_ptr == false) {\n";  	
							feature_processing += "if (PIN_SafeCopy(&arr_addr, (void *)(desc->params[q].position + desc->params[q].offset + base_address), sizeof(ADDRINT)) < sizeof(ADDRINT)) continue;";
						feature_processing +=  "} else {\n";
							feature_processing += "arr_addr = (" + tn + " *)(desc->params[q].position + desc->params[q].offset + base_address);";
						feature_processing +=  "}\n";
				
					// *** Option 3: THE LOCATION IS AN OFFSET FROM THE VALUE OF A REGISTER ***
					feature_processing +=  "} else {\n";
						feature_processing += "if (desc->params[q].is_ptr == false) {\n";  	
							feature_processing += "if (PIN_SafeCopy(&arr_addr, (void *)(input_args[desc->params[q].position] + desc->params[q].offset), sizeof(ADDRINT)) < sizeof(ADDRINT)) continue;";
						feature_processing +=  "} else {\n";
							feature_processing += "arr_addr = (" + tn + " *)(input_args[desc->params[q].position] + desc->params[q].offset);";
						feature_processing +=  "}\n";
					feature_processing +=  "}\n";
				feature_processing +=  "re->add_feature_value_array((" + tn + " *)(arr_addr), desc->params[q].array_dims, &desc->params[q].array_counts);\n";
				feature_processing +=  "}\n";
			}
		} else {
		
			/**
			 *	NOT A BASIC FEATURE
			 *  struct, class, union...
			 */
			// TODO: remove either this, or the same code in create-instrumentation.cc::get_table_entry_for_node
			// HEURISTICS BEGIN //
			bool first = false, last = false, size_found = false;	
			struct member first_m, last_m;
			for (struct member m: type_pair.second) {
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
						utils::log(VL_DEBUG, "There's more than one possible first member, skipping the last");
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
						utils::log(VL_DEBUG, "There's more than one possible last member, skipping the last");
					} else {
						last = true;
						last_m = m;
					}
				}
			}
			if (first && last && first_m.type_name == last_m.type_name) {
				size_found = true;
				artificial_features[t].push_back("size");
			}
			// HEURISTICS BEGIN //
		

			/*********
			*
			*	PHASE 1: FIND THE ACTUAL LOCATION OF THE OBJECT IN MEMORY
			*
			*********/
			feature_processing += "ADDRINT foo = 0;\nADDRINT struct_base_address = 0 + foo;\n";
			
			// *** Option 3: THE LOCATION IS AN OFFSET FROM THE VALUE OF A REGISTER ***
			feature_processing +=  "if (desc->params[q].is_reg_with_addr && desc->params[q].is_addr) {\n";
				feature_processing +=  "if (desc->params[q].is_ptr) {"; 
					// The absolute address points to a ptr to the base of the struct
					feature_processing += "if (PIN_SafeCopy(&struct_base_address, (void *)(input_args[desc->params[q].position] + desc->params[q].offset), sizeof(ADDRINT)) < sizeof(ADDRINT)) continue;";
				feature_processing +=  "}\n else "; 
					// The absolute address points directly to the base of the struct
					feature_processing +=  "struct_base_address = input_args[desc->params[q].position] + desc->params[q].offset;\n"; 			
			// *** THE LOCATION IS AN ADDRESS ***
			feature_processing +=  "} else if (desc->params[q].is_addr) {\n"; 			
				feature_processing +=  "if (desc->params[q].is_ptr) {"; 
					// The absolute address points to a ptr to the base of the struct
					feature_processing += "if (PIN_SafeCopy(&struct_base_address, (void *)(desc->params[q].position + desc->params[q].offset + base_address), sizeof(ADDRINT)) < sizeof(ADDRINT)) continue;";
				feature_processing +=  "}\n else "; 
					// The absolute address points directly to the base of the struct
					feature_processing +=  "struct_base_address = desc->params[q].position + desc->params[q].offset + base_address;\n"; 			
			
			// *** THE LOCATION IS A REGISTER ***
			feature_processing +=  "} else {\n"; 			
				feature_processing +=  "if (desc->params[q].is_ptr) {"; 
					// The absolute address points to a ptr to the base of the struct
					feature_processing += "struct_base_address = input_args[desc->params[q].position] + desc->params[q].offset;";
				feature_processing +=  "}\n else {\n"; 
					// The absolute address points directly to the base of the struct
#ifdef WITH_STRUCT_IN_REG
					feature_processing +=  "// this thing is probably in registers\n";
					bool can_add_size;
					// TODO: can I identify a threshold for which trying to read from registers does not make sense?
					for (struct member m: type_pair.second) 
						feature_processing += code_generator::get_complex_feature_processing_text_from_reg(m, used_names, can_add_size);
					if (size_found && can_add_size) 
						feature_processing += "re->add_feature_value(" + utils::copy_validate_f_name(last_m.feat_names.front() + "R") + " - " + utils::copy_validate_f_name(first_m.feat_names.front() + "R") + ");";
					if (size_found && !can_add_size)
						feature_processing += "printf(\"WARN: trying to parse a structure that does not fit in registers, data collected is probably wrong!\");";
					feature_processing +=  "continue;\n"; 			
#endif
			feature_processing +=  "}\n"; 			
			feature_processing +=  "}\n"; 			
			// now I have the base of the structure
			/*********
			*
			*	PHASE 2: READ DATA RELATIVELY TO THE BEGINNING OF THE OBJECT
			*
			*********/

			// POLYMORPHISM: check possible runtime types
			std::string sym_name = type_pair.first.substr(type_pair.first.find(" ") + 1, std::string::npos);
			utils::log(VL_DEBUG, "Dynamic Types | " + std::to_string(de->hierarchy_tree_nodes_map.size()) + " members; looking for " + sym_name); 

			can_add_size = false;
			// Whatever follows should not change anymore
			if (de->hierarchy_tree_nodes_map.find(sym_name) == de->hierarchy_tree_nodes_map.end()) {
				utils::log(VL_ERROR, "Hierarchy tree not found for " + sym_name + ", bailing out");
				exit(-1);
			} else {
				utils::log(VL_DEBUG, std::to_string(de->hierarchy_tree_nodes_map.at(sym_name)->maybe_real_type.size()) + " possible dynamic types for " + sym_name);
				if (de->hierarchy_tree_nodes_map.at(sym_name)->maybe_real_type.size()) {
					// This thing should have a _vptr (probably inherited)
					utils::log(VL_DEBUG, sym_name + " members:"); 
					bool first_vptr = true;
					for (struct member m: type_pair.second) {
						utils::log(VL_DEBUG, sym_name + " " + m.feat_names.front()); 
						if (m.feat_names.front().find("_vptr") == 0) {
							// here I have what I need
							utils::log(VL_DEBUG, "Found _vptr"); 
							feature_processing += "if (PIN_SafeCopy(&foo, (void *)(struct_base_address + " + std::to_string(m.offset[0]) + "), sizeof(ADDRINT)) < sizeof(ADDRINT)) {re->add_runtime_type(0); continue; }\n";
							if (first_vptr) {
								feature_processing += "char * ro_type_name;\n";
								feature_processing += "uint64_t freudhash;\n";
								first_vptr = false;
							} 

							feature_processing += "ro_type_name = (char *)(*((unsigned long *)((*((unsigned long *)(foo - 8))) + 8)));\n";
							feature_processing += "freudhash = freud_hash((const unsigned char *)ro_type_name);\n";
							feature_processing += "re->add_runtime_type(freudhash);\n";
							feature_processing += "switch (freudhash) {\n";
							create_switch_cases_dfs(de, de->get_class_graph_size(sym_name), feature_processing, sym_name, used_names, 0);
							feature_processing += "}\n";
						}
					}
					if (first_vptr == true) {
						// Ouch
						feature_processing += "// This is a polymorphic type, but I could not find the _vptr member; try to increase the search depth!\n";
						utils::log(VL_ERROR, "Could not find _vptr for " + sym_name + ", this is probably causing problems. Increase the max_depth!");
					}
				} else {
					unsigned int prev_size = feature_processing.size();
					for (struct member m: type_pair.second) 
						feature_processing += code_generator::get_complex_feature_processing_text_from_addr(m, used_names);
					if (feature_processing.size() > prev_size)
						can_add_size = true;
					if (size_found && can_add_size) 
						feature_processing += "re->add_feature_value(" + utils::copy_validate_f_name(last_m.feat_names.front()) + " - " + utils::copy_validate_f_name(first_m.feat_names.front()) + ");";
				}
			}
			
		}
		feature_processing += "break; }\n";
	}
	
	feature_processing += "}\n"; // end of the switch

	signatures += "\n}";
	fprocess << feature_processing; 
	fprocess.close();
}

/* 
* Create the switch cases in the generated code
* Explore the class graph DFS to find all the possibilities
*/
void code_generator::create_switch_cases_dfs(const dwarf_explorer * de, const int tree_size, std::string &feature_processing, const std::string sym_name, std::unordered_set<std::string> & used_names, const int offset) {
	std::string h, tmp;
	if (tree_size > 1) {
		tmp = de->hierarchy_tree_nodes_map.at(sym_name)->linkage_name;
	} else {
		// If I need to actually check for polymorphism, need to "mangle" the symbol name
		// to match what I am going to read from __class_type_info
		tmp = std::to_string(sym_name.size()) + sym_name; 
	}

	std::string tmp_feature_processing = "";
	bool some_feat = false;
	utils::log(VL_DEBUG, "Going DFS for " + sym_name + " a.k.a. " + tmp);
	if (tmp != "notfound") {
		h = std::to_string(utils::hash((unsigned char *)tmp.c_str()));
		// Put some comments in the generated code for readability for debugging purposes
		tmp_feature_processing += "// DTYPE " + sym_name + "\n";
		tmp_feature_processing += "case " + h + "lu: {\n";
		if (offset != 0)
			tmp_feature_processing += "struct_base_address += " + std::to_string(offset) + ";\n";
		tmp = "class " + sym_name; 
		unsigned int prev_size = tmp_feature_processing.size();
		if (de->types.find(tmp) != de->types.end()) {
			// get the actual C code for reading data
			for (struct member m: de->types.at(tmp)) { 
				tmp_feature_processing += code_generator::get_complex_feature_processing_text_from_addr(m, used_names);
			}
		} else {
			utils::log(VL_DEBUG, "Warning: could not find members for " + tmp);
		}
		if (tmp_feature_processing.size() > prev_size)
			some_feat = true;
		tmp_feature_processing += "break;}\n";
	}

	if (some_feat)
		feature_processing += tmp_feature_processing;

	// Go down in the class graph 
	struct hierarchy_tree_node * htn = de->hierarchy_tree_nodes_map.at(sym_name);
	for (hierarchy_tree_node * htn_c: htn->maybe_real_type) {
		create_switch_cases_dfs(de, tree_size, feature_processing, htn_c->class_name, used_names, offset + de->hierarchy_tree_nodes_map.at(sym_name)->offsets.at(htn_c->class_name));
	}
}

