#ifndef DWARF_EXPLORER_HH_INCLUDED
#define DWARF_EXPLORER_HH_INCLUDED

#include <fcntl.h>
#include <string>
#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <list>
#include <queue>
#include <cstdint>

#include "elf++.hh"
#include "dwarf++.hh"
#include "structures.hh"

class dwarf_explorer {
private:
	unsigned int max_depth, max_features;
	std::string binary_filename;
	std::unordered_map<std::string, std::string> class_definitions_map;
	std::vector<struct parameter> cu_variables;
	std::unordered_set<std::string> undefined_types;

	// Explore the dwarf debugging tree to find the definition of a specific class/structure
	// The information collected is returned directly to the caller
	bool walk_tree_definition(const dwarf::die &node, const dwarf::die &parent, const dwarf::line_table lt, std::string name, std::vector<uint32_t> ptr_vec, std::unordered_set<std::string> &visited_structs, std::list<struct member> &members, std::vector<uint32_t> offsets, const std::string current_name, bool is_array, struct array_info &ai, bool is_aligned_membuf, std::string amembuf_type, unsigned int &num_features, const std::string & cuname, std::string & complex_type, bool & lname_found, std::string & lname);

	// Extract the info about the type of the object "node"
	// The information collected is returned directly to the caller
	std::string get_par_type(const dwarf::die &node, unsigned int &local_is_ptr, std::vector<uint32_t> ptr_vec, std::unordered_set<std::string> &visited_structs, std::list<struct member> &members, std::vector<uint32_t> offsets, const std::string current_name, bool is_array, struct array_info &ai, bool is_aligned_membuf, std::string amembuf_type, unsigned int &num_features, const bool decl, bool & lname_found, std::string & lname);

	// Find all the "formal_parameters" that are passed to node
	void dump_parameters(const dwarf::die &node, std::string fname, uint64_t address);
	
	// Find all the global variables in the compilation unit cu_node
	void dump_cu_variables(const dwarf::die &cu_node, std::string cu_name);

	std::string get_class_name(const dwarf::die &node, std::string & lname);

	std::string get_class_linkage_name(const std::string & tname, const dwarf::die &node);

public:
	// TODO: add setters/getters for these objects
	std::map<std::string, std::vector<struct parameter>> func_pars_map;
	std::unordered_map<std::string, std::list<struct member>> types;
	std::unordered_map<std::string, hierarchy_tree_node *> hierarchy_tree_nodes_map;
	std::unordered_map<std::string, uint64_t> func_addr_map;

	// Constructor
	dwarf_explorer(std::string binary_path, const unsigned int depth, const unsigned int features);

	// Returns true if we do not have any information about tname type
	bool type_is_undefined(const std::string tname) const;

	// Returns true if we extracted some info about our symbols of interest
	bool found_info() const { return !func_pars_map.empty(); };

	// Compute the number of possible dynamic types of the type tname
	unsigned int get_class_graph_size(const std::string tname) const;

	// Parse the debugging information to find the definition of the class/struct cname 
	bool find_definition(const std::string & cname, std::vector<uint32_t> ptr_vec, std::unordered_set<std::string> &visited_structs, std::list<struct member> &members, std::vector<uint32_t> offsets, const std::string current_name, bool is_array, struct array_info &ai, bool is_aligned_membuf, std::string amembuf_type, unsigned int &num_features, std::string & complex_type);

	// Scan the debugging symbols to find the possible features for the parameters 
	// and global variables for each symbol in symbols_wl
#ifdef WITH_GLOBAL_VARIABLES
	void walk_tree_dfs(const dwarf::die node, const dwarf::line_table lt, const dwarf::die last_cu, const std::unordered_set<std::string> & symbols_wl, const std::unordered_set<std::string> & symbols_bl);
#else
	void walk_tree_dfs(const dwarf::die node, const dwarf::line_table lt, const std::unordered_set<std::string> & symbols_wl, const std::unordered_set<std::string> & symbols_bl);
#endif
};

#endif
