#ifndef CODE_GEN_HH
#define CODE_GEN_HH

#include "structures.hh"
#include "dwarf-explorer.hh"

class code_generator {
private:
	static void create_switch_cases_dfs(const dwarf_explorer * de, const int tree_size, std::string & feature_processing, const std::string sym_name, std::unordered_set<std::string> & used_names, const int offset, bool & need_arr);
	static std::string get_complex_feature_processing_text_from_addr(const struct member &m, std::unordered_set<std::string> &used_names, bool & need_arr); 
	static std::string get_complex_feature_processing_text_from_reg(const struct member &m, std::unordered_set<std::string> &used_names, bool &can_add_size);

public:
	static void create_instrumentation_code(const dwarf_explorer * de, std::string fprocessfname);
};

#endif
