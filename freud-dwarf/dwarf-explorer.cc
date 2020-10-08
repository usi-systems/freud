#include "dwarf-explorer.hh"
#include "instr-expr-context.hh"
#include <unistd.h>


bool ctx_used;

dwarf_explorer::dwarf_explorer(std::string binary_path, const unsigned int depth, const unsigned int features) {
	binary_filename = binary_path;
	max_depth = depth;
	max_features = features;
}

// Explore the dwarf debugging tree to find the definition of a specific class/structure
// The information collected is returned directly to the caller
bool dwarf_explorer::find_definition(const std::string & cname, std::vector<uint32_t> ptr_vec, std::unordered_set<std::string> &visited_structs, std::list<struct member> &members, std::vector<uint32_t> offsets, const std::string current_name, bool is_array, struct array_info &ai, bool is_aligned_membuf, std::string amembuf_type, unsigned int &num_features, std::string & complex_type) {
        // Open again the file with another descriptor. 
	//TODO: use the same descriptor that we have already
	int fd = open(binary_filename.c_str(), O_RDONLY);
        elf::elf ef(elf::create_mmap_loader(fd));
        dwarf::dwarf dw(dwarf::elf::create_loader(ef));

	// Check if we know already the compilation unit that contains this definition
	bool found_prev = false;
	std::string target_cu_name = "";
	if (class_definitions_map.find(cname) != class_definitions_map.end()) {
		found_prev = true;
		target_cu_name = class_definitions_map[cname];
	}

	// Find and open the compilation unit with the definition
        for (auto cu : dw.compilation_units()) {
		std::string cuname = to_string(cu.root()[dwarf::DW_AT::name]);
		if (found_prev && cuname != target_cu_name)
			continue;

		dwarf::line_table lt = cu.get_line_table();
		dwarf::die res;
		bool lname_found = false;
		std::string lname;

		// We have the CU, now parse the CU to get the actual data
		if (walk_tree_definition(cu.root(), cu.root(), lt, cname, ptr_vec, visited_structs, members, offsets, current_name, is_array, ai, is_aligned_membuf, amembuf_type, num_features, cuname, complex_type, lname_found, lname)) {
			close(fd);
			return true;
		}
        }

	// Ouch, we couldn't find the definition anywhere
	utils::log(VL_ERROR, "The definition for " + cname + " is nowhere to be found!");
	undefined_types.insert(cname);
	class_definitions_map[cname] = "noop"; // this hopefully in never going to be the name of a real CU
	close(fd);
	return false;
}


// Parse a specific compilation unit to find the definition of a class/struct
bool dwarf_explorer::walk_tree_definition(const dwarf::die &node, const dwarf::die &parent, const dwarf::line_table lt, std::string target_name, std::vector<uint32_t> ptr_vec, std::unordered_set<std::string> &visited_structs, std::list<struct member> &members, std::vector<uint32_t> offsets, const std::string current_name, bool is_array, struct array_info &ai, bool is_aligned_membuf, std::string amembuf_type, unsigned int &num_features, const std::string & cuname, std::string & complex_type, bool & lname_found, std::string & lname)
{
	// Only interested in class/struct, which are not declarations
	if ((node.tag == dwarf::DW_TAG::class_type || node.tag == dwarf::DW_TAG::structure_type) && !node.has(dwarf::DW_AT::declaration) && node.has(dwarf::DW_AT::name)) {
		std::string cname = to_string(node[dwarf::DW_AT::name]);

		// While we're here, add the information to our cache
		if (class_definitions_map.find(cname) == class_definitions_map.end()) {
			class_definitions_map.insert(std::make_pair(cname, cuname));
		}

		// If we wound the definition of interest, parse it
		if (target_name == cname) {
			if (node.tag == dwarf::DW_TAG::class_type)
				complex_type = "class ";
			else if (node.tag == dwarf::DW_TAG::structure_type)
				complex_type = "struct ";
			else
				complex_type = "";
			for (auto kid: node) {
				// It's a complex type, it might contain multiple, different, arrays 
				ai.dims = 0;
				ai.counts.clear();

				// We found an actual member of the object
				if (kid.tag == dwarf::DW_TAG::member) {
					if (kid.has(dwarf::DW_AT::name) && kid.has(dwarf::DW_AT::data_member_location)) {
						// if the following goes recursively for real, then I have a problem; need to compute the address of this thing to
						// find the real address of its members
						// TODO: handle recursive types!
						std::string local_name = kid[dwarf::DW_AT::name].as_string();
						if (local_name == "next" || local_name == "prev") {
							utils::log(VL_DEBUG, "Do I have a list?");
							continue;
						} else if (local_name.find("_vptr") == 0) {
							// Found a vtable pointer
							struct member m;
							m.feat_names.push_back(local_name);
							m.type_name = "enum unsigned long";
							std::vector<unsigned int> ocopy(offsets);
							ocopy.push_back(kid[dwarf::DW_AT::data_member_location].as_uconstant());
							m.offset = ocopy;
							m.pointer = ptr_vec;
							m.ai = ai;
							members.push_back(m);
							num_features++;
							continue;
						} 

						// Get name and offset
						local_name = current_name + "." + local_name;
						std::vector<unsigned int> ocopy(offsets);
						ocopy.push_back(kid[dwarf::DW_AT::data_member_location].as_uconstant());
						unsigned int fooptr;
						bool lname_found;
						std::string lname;
						// Explore the type (which might still be complex)
						std::string type = dwarf_explorer::get_par_type(kid, fooptr, ptr_vec, visited_structs, members, ocopy, local_name, is_array, ai, is_aligned_membuf, amembuf_type, num_features, false, lname_found, lname);
					} else {
						utils::log(VL_DEBUG, "Found a member, but no name or data_member_location!");
						if (!kid.has(dwarf::DW_AT::name))
							utils::log(VL_DEBUG, "name is missing");
						if (!kid.has(dwarf::DW_AT::data_member_location))
							utils::log(VL_DEBUG, "data_member_location is missing");
					}
				} else if (kid.tag == dwarf::DW_TAG::inheritance) {
					uint32_t offset = 0;
					if (kid.has(dwarf::DW_AT::data_member_location)) {
						if (kid[dwarf::DW_AT::data_member_location].get_type() == dwarf::value::type::uconstant ||
								kid[dwarf::DW_AT::data_member_location].get_type() == dwarf::value::type::constant ||
								kid[dwarf::DW_AT::data_member_location].get_type() == dwarf::value::type::sconstant) {

							// Assuming to ::constant means unsigned. This is not really clear from the dwarf manual
							if (kid[dwarf::DW_AT::data_member_location].get_type() == dwarf::value::type::sconstant)
								offset = kid[dwarf::DW_AT::data_member_location].as_sconstant();
							else
								offset = kid[dwarf::DW_AT::data_member_location].as_uconstant();
							std::vector<unsigned int> ocopy(offsets);
							ocopy.push_back(offset);
							// I pass ptr here, but it's never gonna be incremented
							// type is mandatory...
							unsigned int fooptr;
							dwarf_explorer::get_par_type(kid, fooptr, ptr_vec, visited_structs, members, ocopy, current_name, is_array, ai, is_aligned_membuf, amembuf_type, num_features, false, lname_found, lname);	
						} else {
							// TODO: the actual location is an addr, should parse also this...
							utils::log(VL_DEBUG, "The location of the inheritance is not a constant, skipping. " + to_string(kid[dwarf::DW_AT::data_member_location].get_type()));
							continue;
						}
					}
				}
				else if (kid.tag == dwarf::DW_TAG::class_type || kid.tag == dwarf::DW_TAG::structure_type) {
					utils::log(VL_DEBUG, "Skipping nested class/struct");
				}
				// I want to avoid cycles only going down in the recursion, not breadth-wise
				visited_structs.clear();
			}
			// I found (and hopefully parsed) the definition of the class
			return true;
		}
	}

	// Go down, DFS 
	for (auto &child : node)
		if (walk_tree_definition(child, node, lt, target_name, ptr_vec, visited_structs, members, offsets, current_name, is_array, ai, is_aligned_membuf, amembuf_type, num_features, cuname, complex_type, lname_found, lname))
			return true;
	
	return false;
}

std::string dwarf_explorer::get_par_type(const dwarf::die &node, unsigned int &local_is_ptr, std::vector<uint32_t> ptr_vec, std::unordered_set<std::string> &visited_structs, std::list<struct member> &members, std::vector<uint32_t> offsets, const std::string current_name, bool is_array, struct array_info &ai, bool is_aligned_membuf, std::string amembuf_type, unsigned int &num_features, const bool decl, bool & lname_found, std::string & lname) {
	static std::unordered_set<std::string> analyzed_types;
	
	uint32_t ptr = 0;
	bool is_enum = false;
	bool do_not_explore = false;
	
	dwarf::die type_die;
	if (node.has(dwarf::DW_AT::type))
		type_die = node[dwarf::DW_AT::type].as_reference();
	else
		return "";

	// Continue exploring (following references) until we reach something concrete
	// Like a basic type or an object
	while (type_die.tag != dwarf::DW_TAG::base_type && 
			type_die.tag != dwarf::DW_TAG::class_type &&
			type_die.tag != dwarf::DW_TAG::structure_type &&
			type_die.tag != dwarf::DW_TAG::union_type &&
			type_die.tag != dwarf::DW_TAG::subroutine_type
			) {
		if (type_die.tag == dwarf::DW_TAG::pointer_type || type_die.tag == dwarf::DW_TAG::reference_type) {
			// We are traversing a pointer, need to remember this for future dereferencing
			ptr++;
			if (!type_die.has(dwarf::DW_AT::type)) { 
				utils::log(VL_DEBUG, "Untyped pointer not handled");
				return "";
			} 
		} else if (type_die.tag == dwarf::DW_TAG::const_type) {
			// nothing to do, we do not care about const
			if (!type_die.has(dwarf::DW_AT::type)) {
				utils::log(VL_DEBUG, "Untyped const not handled");
				return "";
			}
		} else if (type_die.tag == dwarf::DW_TAG::enumeration_type) {
			is_enum = true;
			if (is_array) {
				utils::log(VL_DEBUG, "Arrays of enums not handled yet");
				return "";
			}
		} else if (type_die.tag == dwarf::DW_TAG::array_type && !is_aligned_membuf) {
			utils::log(VL_DEBUG, "Found array! " + to_string(node[dwarf::DW_AT::name]));
			is_array = true;
			ptr++; // Well, an array IS pointer after all
			// Let's see if there is the subrange info, with the size of the array
			for (auto kid: type_die) {
				if (kid.tag == dwarf::DW_TAG::subrange_type) {
					// I'm not expecting any lower boundary, and assuming 0 implicitly (DWARF-4, Sec. 5.11)
					if ((kid.has(dwarf::DW_AT::lower_bound) && kid[dwarf::DW_AT::lower_bound].as_sconstant()) ||
						kid.has(dwarf::DW_AT::threads_scaled)) { // don't know what to do with this, see Sec. 5.11 of the manual
						utils::log(VL_ERROR, "Lower bounds (or threads scaled) on arrays not handled, bailing out!");
						exit(-1);
					}
					// It's not clear from the manual if it's a signed or unsigned constant
					// It seems to be signed, but that does not really make sense...
					if (kid.has(dwarf::DW_AT::upper_bound)) {
						ai.counts.push_back(kid[dwarf::DW_AT::upper_bound].as_uconstant());
						utils::log(VL_DEBUG, "Found subrange: " + std::to_string(kid[dwarf::DW_AT::upper_bound].as_uconstant()));
						ai.dims++;
					} else if (kid.has(dwarf::DW_AT::count)) {
						ai.counts.push_back(kid[dwarf::DW_AT::count].as_uconstant());
						utils::log(VL_DEBUG, "Found subrange with count");
						ai.dims++;
					}
				}
			}
		} else if (type_die.tag == dwarf::DW_TAG::typedef_) {
			// Nothing to do? 
		} else if (type_die.tag == dwarf::DW_TAG::ptr_to_member_type) {
			utils::log(VL_DEBUG, "FIXME, ignoring ptr_to_mem!");
			return "";
		}
		if (!type_die.has(dwarf::DW_AT::type)) // may happen, as example with 0x3b, unspecified type
			// FIXME: no type?
			return "";
		type_die = type_die[dwarf::DW_AT::type].as_reference();
	}

	local_is_ptr = ptr;
	ptr_vec.push_back(ptr);
	std::string res= "";
	
	if (!type_die.has(dwarf::DW_AT::name)) { 
		// FIXME: noname?
		return "";
	}
	
	std::string c_typename = to_string(type_die[dwarf::DW_AT::name]);
	bool vtable = false;
	analyzed_types.insert(c_typename);

	if (type_die.tag == dwarf::DW_TAG::subroutine_type) {
		// FIXME: subrtn?
		utils::log(VL_DEBUG, "subroutine_type not handled");
		return "";
	}
	else if (type_die.tag == dwarf::DW_TAG::union_type) {
		// FIXME: union?
		utils::log(VL_DEBUG, "union_type not handled");
		return "";
	}
	// If it is a complex type, explore it
	else if ((type_die.tag == dwarf::DW_TAG::structure_type ||
			type_die.tag == dwarf::DW_TAG::class_type) && do_not_explore == false) {
		if (is_array) {
			// Just pretend it's not an array, this will consider only the first element
			utils::log(VL_DEBUG, "Arrays of complex types not handled yet");
			is_array = false;
		}
		// Break cycles (and btw, if this is the case, I bet this is a list)
		std::string tname = "";
		std::string lname = "";
		if (type_die.has(dwarf::DW_AT::name))
			tname = type_die[dwarf::DW_AT::name].as_string();
		if (tname.substr(0, 17) == "__aligned_membuf<") {
			is_aligned_membuf = true;
			amembuf_type = tname.substr(17, tname.size() - 1 - 17);
			is_array = false;
		}
		utils::log(VL_DEBUG, "VISITING " + tname);
		// Debugging information about possible dynamic types
		if (hierarchy_tree_nodes_map.find(tname) != hierarchy_tree_nodes_map.end()) {
			if (hierarchy_tree_nodes_map.at(tname)->maybe_real_type.size())
				utils::log(VL_DEBUG, "... which could actually be (1st level only): " + std::to_string(hierarchy_tree_nodes_map.at(tname)->maybe_real_type.size())); 
			for (hierarchy_tree_node * tn: hierarchy_tree_nodes_map.at(tname)->maybe_real_type) {
				utils::log(VL_DEBUG, tn->class_name);
			}
		}
		if (ptr_vec.size() > max_depth || num_features > max_features) {
			// do not go too deep with the exploration for new features
			utils::log(VL_DEBUG, "STOP beacause we're going too deep or have too many features; depth: " + std::to_string(ptr_vec.size()) + "; feat: " + std::to_string(num_features));
			return "";
		}
		if (tname == "" || visited_structs.find(tname) != visited_structs.end()) {
			utils::log(VL_DEBUG, "STOP to avoid recursion: " + tname);
			return "";
		}
		visited_structs.insert(tname);
		
		if (type_die.tag == dwarf::DW_TAG::structure_type)
				res += "struct ";
		else if (type_die.tag == dwarf::DW_TAG::class_type)
				res += "class ";
		if (decl && 
			type_die.has(dwarf::DW_AT::declaration)) {
			utils::log(VL_DEBUG, "Found a ref to class declaration. Need to find the definition.");
			std::string empty_str;
			find_definition(to_string(type_die[dwarf::DW_AT::name]), ptr_vec, visited_structs, members, offsets, current_name, is_array, ai, is_aligned_membuf, amembuf_type, num_features, empty_str);	
		}
		else {
			// FIXME: remove this code duplication with the code in find_definition!
			for (auto kid: type_die) {
				// It's a complex type, it might contain multiple, different, arrays 
				ai.dims = 0;
				ai.counts.clear();
				if (kid.tag == dwarf::DW_TAG::member) {
					if (kid.has(dwarf::DW_AT::name) && kid.has(dwarf::DW_AT::data_member_location)) {
						// if the following goes recursively for real, then I have a problem; need to compute the address of this thing to
						// find the real address of its members
						// TODO: handle recursive types!
						std::string local_name = kid[dwarf::DW_AT::name].as_string();
						if (local_name == "next" || local_name == "prev") {
							utils::log(VL_DEBUG, "Do I have a list?");
							continue;
						} else if (local_name.find("_vptr") == 0) {
							// Found a vtable pointer
							struct member m;
							m.feat_names.push_back(local_name);
							m.type_name = "enum unsigned long";
							std::vector<unsigned int> ocopy(offsets);
							ocopy.push_back(kid[dwarf::DW_AT::data_member_location].as_uconstant());
							m.offset = ocopy;
							m.pointer = ptr_vec;
							m.ai = ai;
							members.push_back(m);
							num_features++;
							continue;
						} 

						
						// Get name and offset
						local_name = current_name + "." + local_name;
						std::vector<unsigned int> ocopy(offsets);
						ocopy.push_back(kid[dwarf::DW_AT::data_member_location].as_uconstant());
						unsigned int fooptr;
						bool lname_found;
						std::string lname;
						// Explore the type (which might still be complex)
						std::string type = dwarf_explorer::get_par_type(kid, fooptr, ptr_vec, visited_structs, members, ocopy, local_name, is_array, ai, is_aligned_membuf, amembuf_type, num_features, false, lname_found, lname);
					} else {
						utils::log(VL_DEBUG, "Found a member, but no name or data_member_location!");
						if (!kid.has(dwarf::DW_AT::name))
							utils::log(VL_DEBUG, "name is missing");
						if (!kid.has(dwarf::DW_AT::data_member_location))
							utils::log(VL_DEBUG, "data_member_location is missing");
					}
				} else if (kid.tag == dwarf::DW_TAG::inheritance) {
					uint32_t offset = 0;
					if (kid.has(dwarf::DW_AT::data_member_location)) {
						if (kid[dwarf::DW_AT::data_member_location].get_type() == dwarf::value::type::uconstant ||
								kid[dwarf::DW_AT::data_member_location].get_type() == dwarf::value::type::constant ||
								kid[dwarf::DW_AT::data_member_location].get_type() == dwarf::value::type::sconstant) {

							// Assuming to ::constant means unsigned. This is not really clear from the dwarf manual
							if (kid[dwarf::DW_AT::data_member_location].get_type() == dwarf::value::type::sconstant)
								offset = kid[dwarf::DW_AT::data_member_location].as_sconstant();
							else
								offset = kid[dwarf::DW_AT::data_member_location].as_uconstant();
							std::vector<unsigned int> ocopy(offsets);
							ocopy.push_back(offset);
							// I pass ptr here, but it's never gonna be incremented
							// type is mandatory...
							unsigned int fooptr;
							dwarf_explorer::get_par_type(kid, fooptr, ptr_vec, visited_structs, members, ocopy, current_name, is_array, ai, is_aligned_membuf, amembuf_type, num_features, false, lname_found, lname);	
						} else {
							// TODO: the actual location is an addr, should parse also this...
							utils::log(VL_DEBUG, "The location of the inheritance is not a constant, skipping. " + to_string(kid[dwarf::DW_AT::data_member_location].get_type()));
							continue;
						}
					}
				}
				else if (kid.tag == dwarf::DW_TAG::class_type || kid.tag == dwarf::DW_TAG::structure_type) {
					utils::log(VL_DEBUG, "Skipping nested class/struct");
				}
				// I want to avoid cycles only going down in the recursion, not breadth-wise
				visited_structs.clear();
			}
		}
	}
	else if (is_enum)
		res += "enum ";
	else if (is_array) {
		res += "array ";
	}

	// Not a class, nor structure... should be a primitive type
	// which also means that we reached the base case in the recursion
	res += to_string(type_die[dwarf::DW_AT::name]);

	// If this primitive type is part of a class, add the information to the relative
	// data structure
	if (res.find("struct ") > 0 && res.find("class ") > 0) {
		struct member m;
		m.feat_names.push_back(current_name);
		if (is_array)
			m.feat_names.push_back(".artificial" + current_name + "_aggr");
		if (is_aligned_membuf)
			m.type_name = amembuf_type;
		else
			m.type_name = res;
		if (current_name == "" || m.type_name == "")
			return res;
		m.offset = offsets;
		m.pointer = ptr_vec;
		m.ai = ai;
		members.push_back(m);
		num_features++;
	}
	return res;
}

void dwarf_explorer::dump_cu_variables(const dwarf::die &cu_node, std::string cu_name) {
	for (auto &child : cu_node) {
		/**
		 *	VARIABLES
		 */
		std::string name;
		if (child.tag == dwarf::DW_TAG::variable && child.has(dwarf::DW_AT::name) && child.has(dwarf::DW_AT::location)) {	
			std::string vname = to_string(child[dwarf::DW_AT::name]);
			name = to_string(child[dwarf::DW_AT::name]);
		} else {
			// Not what we're looking for
			continue;
		}
		std::vector<unsigned int> ptrv;
		std::vector<unsigned int> offsets;
		std::unordered_set<std::string> visited_structs;
		std::list<struct member> members;
		unsigned int ptr;
		struct array_info ai; ai.dims = 0;
		std::string amembuf_type = "";
		unsigned int num_features = 0;
		std::string linkage_type_name;
		bool lname_found = false;
		std::string type = dwarf_explorer::get_par_type(child, ptr, ptrv, visited_structs, members, offsets, "", false, ai, false, amembuf_type, num_features, true, lname_found, linkage_type_name);
		// if the first par is a > 1 pointer, probably is thing is meant to be allocated by the callee
		if (type == "" || ptr > 1) 
			continue;
		unsigned int acount = 0;
		unsigned int rcount = 0;
		struct parameter p;
		p.name = name;
		p.type_name = type;
		p.linkage_type_name = linkage_type_name;	
		p.ai = ai;
		types[type] = members;
		uint64_t addr = 0;
		if (cu_node.has(dwarf::DW_AT::low_pc))
			addr = cu_node[dwarf::DW_AT::low_pc].as_address();
		instr_expr_context ec(addr);
		dwarf::expr_result result;
		result.value = 0; // suppress gcc warning 
		result.location_type = dwarf::expr_result::type::address;
		result.offset_from_value = 0;
		ctx_used = false;
		std::string eval_str;
		
		if (child[dwarf::DW_AT::location].get_type() == dwarf::value::type::exprloc) 
			result = child[dwarf::DW_AT::location].as_exprloc().evaluate(&ec, eval_str, &cu_node);
		else if (child[dwarf::DW_AT::location].get_type() == dwarf::value::type::loclist)
			result = child[dwarf::DW_AT::location].as_loclist().evaluate(&ec, &cu_node);
		
		if (!ctx_used) {
			switch (result.location_type) {
				case dwarf::expr_result::type::address:
				{
					if (result.offset_from_value == INT64_MAX) {
						utils::log(VL_ERROR, "Could not find the cfa!");
						break;
					}
					p.is_addr = true;
					if (ctx_used) {
						// FIXME: I'm assuming rsp by default, might not be true
						p.is_register_with_addr = true;
						// rsp is actually 7, but in my Pin instrumentation it ends up in the 6th pos in the array of regs
						p.location = 6; 
						p.uses_input = 1;
					} else {
						p.is_register_with_addr = false;
						p.location = result.value;
						p.uses_input = 0;
					}
					acount++;
					p.offset_from_location = result.offset_from_value;
					if (ptr) {
						p.is_ptr = true;
					} else {
						p.is_ptr = false;
					}
					cu_variables.push_back(p);
					break;
				}
				case dwarf::expr_result::type::reg:
				{
					if (ctx_used) {
						utils::log(VL_ERROR, "Reg and ctx is used, bailing out");
						exit(-1);
					}
					std::string val_name = "";
					if (dwarf2amd64[result.value] == -1) {
						utils::log(VL_ERROR, "Cannot resolve reg " + std::to_string(result.value));
						exit(-1);
					}
					p.is_addr = false;
					p.is_register_with_addr = false;
					p.location = dwarf2amd64[result.value];
					p.offset_from_location = result.offset_from_value;
					if (result.value < 17) { // it's an INT register
						p.in_XMM = false;
						if (ptr) {
							val_name = "((void *)r";
							p.is_ptr = true;
						}
						else { // handle the value to ptr conversion, since I could not find a way to get a ref to an INT register
							val_name = "((void *)&tmpr";
							p.is_ptr = false;
						}
					}
					else { // ptr MUST BE 0 
						p.is_ptr = false;
						p.in_XMM = true;
						val_name = "((void *)r";
					}

					rcount++;
					p.uses_input = 1;
					cu_variables.push_back(p);
					break;
				}
				case dwarf::expr_result::type::literal:
				{
					utils::log(VL_ERROR, "Literal " + std::to_string(result.value));
					break;
				}
				case dwarf::expr_result::type::empty:
				{
					utils::log(VL_ERROR, "Empty, which means optimized out? " + std::to_string(result.value));
					break;
				}
				default:
				{
					utils::log(VL_ERROR, "Default reached in dwarf::expr_result::type");
					break;
				}
			} // end of switch
		}	
	}
}

void dwarf_explorer::dump_parameters(const dwarf::die &node, std::string fname, uint64_t address) {
	struct parameter p;
	unsigned int acount = 0;
	unsigned int rcount = 0;
	std::vector<struct parameter> parameters;
	bool has_this = false;
	if (node.has(dwarf::DW_AT::object_pointer))
		has_this = true;
	for (auto &child : node) {
		/*
		 *	FORMAL PARAMETERS
		 */
		if (child.tag == dwarf::DW_TAG::formal_parameter) { // only parameters
			std::string name;	
			if (has_this && child.has(dwarf::DW_AT::artificial)) {
				// this must be the "this" implicit parameter
				// It should implicitly be in rdi (amd64), or reg5 (dwarf)
				name = "this";
			} else {
				if ((!child.has(dwarf::DW_AT::name) || !child.has(dwarf::DW_AT::location))) {
					utils::log(VL_DEBUG, fname + ": skipping formal parameter because of no name or location!");
					continue;
				}
				name = to_string(child[dwarf::DW_AT::name]);
			}
			std::vector<unsigned int> ptrv;
			std::vector<unsigned int> offsets;
			std::unordered_set<std::string> visited_structs;
			std::list<struct member> members;
			unsigned int ptr;
			struct array_info ai; ai.dims = 0;
			std::string amembuf_type = "";
			unsigned int num_features = 0;
			std::string linkage_type_name;
			bool lname_found = false;
			std::string type = dwarf_explorer::get_par_type(child, ptr, ptrv, visited_structs, members, offsets, "", false, ai, false, amembuf_type, num_features, true, lname_found, linkage_type_name);
			// if the first par is a > 1 pointer (and the type is not an array), probably this thing is meant to be allocated by the callee
			if (type == "" || (ptr > 1 && type.find("array") > 0)) 
				continue;

			utils::log(VL_DEBUG, "Found " + name + " par with type " + type + " with members: " + std::to_string(members.size()));
			p.name = name;
			p.type_name = type;
			p.linkage_type_name = linkage_type_name;	
			p.ai = ai;
			types[type] = members;
			instr_expr_context ec(address);
			dwarf::expr_result result;
			result.value = 0; // fake init, to suppress gcc error
			result.offset_from_value = 0;
			result.location_type = dwarf::expr_result::type::address;
			ctx_used = false;
			std::string eval_str;
			if (has_this && child.has(dwarf::DW_AT::artificial)) {
				// this is the "this" parameter, I know it is in reg5 (rdi)
				result.value = 5;
				result.offset_from_value = 0;
				result.location_type = dwarf::expr_result::type::reg;
			} else {
				if (child[dwarf::DW_AT::location].get_type() == dwarf::value::type::exprloc) {
					result = child[dwarf::DW_AT::location].as_exprloc().evaluate(&ec, eval_str, &node);
				}
				else if (child[dwarf::DW_AT::location].get_type() == dwarf::value::type::loclist) {
					result = child[dwarf::DW_AT::location].as_loclist().evaluate(&ec, &node);
				}
			}
			switch (result.location_type) {
				case dwarf::expr_result::type::address:
				{
					if (result.offset_from_value == INT64_MAX) {
						utils::log(VL_ERROR, "Could not find the cfa!");
						break;
					}
					p.is_addr = true;
					if (ctx_used) {
						// FIXME: I'm assuming rsp by default, might not be true
						p.is_register_with_addr = true;
						// rsp is actually 7, but in my Pin instrumentation it ends up in the 6th pos in the array of regs
						// p.location = 6;
						p.location = dwarf2amd64[result.value]; 
						p.uses_input = 1;
					} else {
						p.is_register_with_addr = false;
						p.location = result.value;
						p.uses_input = 0;
					}
					acount++;
					p.offset_from_location = result.offset_from_value;
					if (ptr) {
						p.is_ptr = true;
					} else {
						p.is_ptr = false;
					}
					parameters.push_back(p);
					break;
				}
				case dwarf::expr_result::type::reg:
				{
					if (ctx_used) {
						p.is_register_with_addr = true;
						p.is_addr = true;
					} else {
						p.is_register_with_addr = false;
						p.is_addr = false;
					}
					std::string val_name = "";
					if (dwarf2amd64[result.value] == -1) {
						utils::log(VL_ERROR, "Cannot resolve reg " + std::to_string(result.value));
						exit(-1);
					}
					p.location = dwarf2amd64[result.value];
					p.offset_from_location = result.offset_from_value;
					if (result.value < 17) { // it's an INT register
						p.in_XMM = false;
						if (ptr) {
							val_name = "((void *)r";
							p.is_ptr = true;
						}
						else { // handle the value to ptr conversion, since I could not find a way to get a ref to an INT register
							val_name = "((void *)&tmpr";
							p.is_ptr = false;
						}
					}
					else { // ptr MUST BE 0 
						p.is_ptr = false;
						p.in_XMM = true;
						val_name = "((void *)r";
					}

					rcount++;
					p.uses_input = 1;
					parameters.push_back(p);
					break;
				}
				case dwarf::expr_result::type::literal:
				{
					utils::log(VL_ERROR, "Literal " + std::to_string(result.value));
					break;
				}
				case dwarf::expr_result::type::empty:
				{
					utils::log(VL_ERROR, "Empty, which means optimized out? " + std::to_string(result.value));
					break;
				}
				default:
				{
					utils::log(VL_ERROR, "Default reached in dwarf::expr_result::type");
					break;
				}
			} // end of switch
		}
		//std::cout << "IEND" << std::endl;
	}
		//std::cout << "I AM OUT" << std::endl;
	func_pars_map[fname] = parameters; // TODO: should be safe, but function could be repeated
	if (cu_variables.size() > 0)
		func_pars_map[fname].insert(func_pars_map[fname].end(), cu_variables.begin(), cu_variables.end());
}

void dwarf_explorer::walk_tree_dfs(const dwarf::die node, 
					const dwarf::line_table lt, 
#ifdef WITH_GLOBAL_VARIABLES
					const dwarf::die last_cu, 
#endif
					const std::unordered_set<std::string> & symbols_wl, 
					const std::unordered_set<std::string> & symbols_bl)
{
#ifdef WITH_GLOBAL_VARIABLES
	// TODO: I am assuming that a class/struct die is necessarily going to be at the root level in the CU; is this true?
	// E.g. it is for sure not true for nested class/struct, which are not handled ATM
	if ((node.tag == dwarf::DW_TAG::class_type || node.tag == dwarf::DW_TAG::structure_type) && !node.has(dwarf::DW_AT::declaration) && node.has(dwarf::DW_AT::name)) { 
		// not interested in declarations, yet...
		std::string cname = to_string(node[dwarf::DW_AT::name]);
		//std::cout << "### Found class " << cname << std::endl;
		if (hierarchy_tree_nodes_map.find(cname) == hierarchy_tree_nodes_map.end()) {
			std::string linkage_name = get_class_linkage_name(cname, node);
			hierarchy_tree_nodes_map.insert(std::make_pair(cname, new hierarchy_tree_node(cname, linkage_name)));
		}
		dwarf::die node_cpy(node);
		for (auto &kid: node_cpy) {
			if (kid.tag == dwarf::DW_TAG::inheritance) {
				if (!kid.has(dwarf::DW_AT::data_member_location)) {
					utils::log(VL_ERROR, "Inheritance without data_member_location!");
					exit(-1);
				}
				int offset = 0;

				if (kid.has(dwarf::DW_AT::data_member_location)) {
					if (kid[dwarf::DW_AT::data_member_location].get_type() == dwarf::value::type::uconstant ||
							kid[dwarf::DW_AT::data_member_location].get_type() == dwarf::value::type::constant) { 
						offset = kid[dwarf::DW_AT::data_member_location].as_uconstant();
					}
					else if (kid[dwarf::DW_AT::data_member_location].get_type() == dwarf::value::type::sconstant) { 
						offset = kid[dwarf::DW_AT::data_member_location].as_sconstant();
					}
					else {
						// TODO: the actual location is an addr, should parse also this...
						utils::log(VL_DEBUG, "The location is not a constant, skipping. " + to_string(kid[dwarf::DW_AT::data_member_location].get_type()));
						continue;
					}
				}

				// This class inherits from another. Let's retrieve its name
				std::string link_name;
				std::string parent_name = get_class_name(kid, link_name);
				if (cname != parent_name) {
					if (parent_name != "notfound") {
						if (hierarchy_tree_nodes_map.find(parent_name) == hierarchy_tree_nodes_map.end()) {
							hierarchy_tree_nodes_map.insert(std::make_pair(parent_name, new hierarchy_tree_node(parent_name, link_name)));
						}
						hierarchy_tree_nodes_map.at(cname)->offsets.insert(std::make_pair(parent_name, offset));
						hierarchy_tree_nodes_map.at(parent_name)->maybe_real_type.insert(hierarchy_tree_nodes_map.at(cname));
						hierarchy_tree_nodes_map.at(parent_name)->offsets.insert(std::make_pair(cname, -offset));
					}
				} else {
					utils::log(VL_DEBUG, cname + " -> " + parent_name + "; check CRTP");
				}
			}
		}
		if (class_definitions_map.find(cname) == class_definitions_map.end()) {
			class_definitions_map.insert(std::pair<std::string, std::string>(cname, last_cu[dwarf::DW_AT::name].as_string()));
		}
	} else
#endif
	if (node.tag == dwarf::DW_TAG::subprogram && !node.has(dwarf::DW_AT::declaration)) { // not interested in declarations, yet...
		std::string fname = "";
		if (node.has(dwarf::DW_AT::linkage_name))
			fname = to_string(node[dwarf::DW_AT::linkage_name]);
		else if (node.has(dwarf::DW_AT::name)) 
			fname = to_string(node[dwarf::DW_AT::name]);
		else if (node.has(dwarf::DW_AT::specification)) {
			dwarf::die spec_parent = node[dwarf::DW_AT::specification].as_reference();
			if (spec_parent.has(dwarf::DW_AT::linkage_name))
				fname = to_string(spec_parent[dwarf::DW_AT::linkage_name]);
			else if (spec_parent.has(dwarf::DW_AT::name)) 
				fname = to_string(spec_parent[dwarf::DW_AT::name]);
		}
		else if (node.has(dwarf::DW_AT::abstract_origin)) {
			// this is an inlined function. Do nothing, since pin is not gonna let us hook into this anyway, probably
			// TODO: check if this is correct
		}

		if (symbols_bl.find(fname) == symbols_bl.end()) {
			utils::validate_f_name(fname); // some functions have names containing . !!!	
			if (
			!(
			(symbols_wl.size() > 0 && symbols_wl.find(fname) == symbols_wl.end()) || 
			(symbols_bl.size() > 0 && symbols_bl.find(fname) != symbols_bl.end())
			)
			) {
				// If the symbol doesn't have low_pc, it doesn't have any code, let's skip
				// "If an entity has no associated machine code, none of these attributes are specified."
				// Dwarf-4 manual, page 37
				if (node.has(dwarf::DW_AT::low_pc)) {
					uint64_t addr = 0;
					addr = node[dwarf::DW_AT::low_pc].as_address();
					auto it = lt.find_address_after_prologue(addr);
					if (it == lt.end())
						utils::log(VL_ERROR, "Could not find address after prologue!");
					else
						func_addr_map[fname] = it->address;
#ifdef WITH_GLOBAL_VARIABLES
					// get the global variables in this compile unit
					// but first, clear the old ones
					cu_variables.clear();
					std::string cuname = to_string(last_cu[dwarf::DW_AT::name]);
					dwarf_explorer::dump_cu_variables(last_cu, cuname);
#endif
					dwarf_explorer::dump_parameters(node, fname, it->address);
				} 
			}
		}
		return; // no need to dive deeper
	} 
        
	for (auto child: node) 
#ifdef WITH_GLOBAL_VARIABLES
		walk_tree_dfs(child, lt, last_cu, symbols_wl, symbols_bl);
#else
		walk_tree_dfs(child, node, lt, symbols_wl, symbols_bl);
#endif
}

std::string dwarf_explorer::get_class_linkage_name(const std::string & tname, const dwarf::die &node) {
	// node MUST be either a class or a struct
	if (node.tag != dwarf::DW_TAG::class_type &&
			node.tag != dwarf::DW_TAG::structure_type)
		return "";
	std::string lname;
	for (auto kid: node) {
		// Find the linkage name! Use the destructor, which is usually the first subprogram of the class!
		if (kid.has(dwarf::DW_AT::linkage_name)) {
			// parse until we see the tname in the linkage_name string
			// This should be robust whatever the first method is
			std::string input = kid[dwarf::DW_AT::linkage_name].as_string();
			int iidx = 0, iteration = 0;
			std::string part;
			do {
				part = "";
				// reach the next number
				while (iidx < input.size() && (input[iidx] < '0' || input[iidx] > '9'))
					iidx++;
				// parse the number
				while (iidx < input.size() && (input[iidx] >= '0' && input[iidx] <= '9'))
					part += input[iidx++];
				int nend = iidx;
				if (part.size() == 0)
					return "notfound"; // this string didn't have any number
				// FIXME: handle scalar values in mangled names
				unsigned long long len = std::stoul(part);
				if (nend + len >= input.size()) {
					// this happens for primitive types with special encodings
					// I don't need this information anyway
					return "notfound";
				}
				part = input.substr(nend, len);
				lname += std::to_string(part.size()) + part;
			// Do not do exact match because of templates!
			} while (tname.find(part) == std::string::npos && ++iteration);
			// for some reason, this should equal whatever is going to be read by the instr
			if (iteration > 0)
				lname = "N" + lname + "E";
			return lname;	
		}
	}
	return "notfound";
}

std::string dwarf_explorer::get_class_name(const dwarf::die &node, std::string & lname) {
	dwarf::die type_die;
	if (node.has(dwarf::DW_AT::type))
		type_die = node[dwarf::DW_AT::type].as_reference();
	else
		return "";
	
	// I want to find a class or a struct
	while (type_die.tag != dwarf::DW_TAG::class_type &&
			type_die.tag != dwarf::DW_TAG::structure_type)
		type_die = type_die[dwarf::DW_AT::type].as_reference();
	
	// Here I am either in a class, or in a struct
	if (type_die.has(dwarf::DW_AT::name)) {
		std::string cname = type_die[dwarf::DW_AT::name].as_string();
		lname = get_class_linkage_name(cname, type_die);
		return cname;
	}
	return "";
}

unsigned int dwarf_explorer::get_class_graph_size(const std::string tname) const {
	// GO BFS, ITERATIVELY
	std::queue<const struct hierarchy_tree_node *> q;
	q.push(hierarchy_tree_nodes_map.at(tname));
	unsigned int size = 0;
	while (!q.empty()) {
		const struct hierarchy_tree_node * curr = q.front();
		q.pop();
		size++;
		for (const struct hierarchy_tree_node * mrt: curr->maybe_real_type) {
			q.push(mrt);
		}
	}
	return size;
}

bool dwarf_explorer::type_is_undefined(const std::string tname) const {
	return undefined_types.find(tname) != undefined_types.end();
}

