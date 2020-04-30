#ifndef TABLE_GENERATOR_HH_INCLUDED
#define TABLE_GENERATOR_HH_INCLUDED

#include "structures.hh"
#include "dwarf-explorer.hh"
#include <fstream>

class table_generator {
private:
	static void create_table_entries_for_classes_dfs(dwarf_explorer * de, int tree_size, std::string &descriptors_code, const std::string sym_name, const std::string ppname);
	static std::string get_table_entry_for_node(dwarf_explorer * de, int tree_size, const struct hierarchy_tree_node * mrt, const std::string ppname);

public:
	static void create_table(dwarf_explorer * de, std::string tbl_filename);
};

#endif
