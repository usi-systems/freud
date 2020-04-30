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

#include "elf++.hh"
#include "dwarf++.hh"

#include "structures.hh"
#include "utils.hh"
#include "code-gen.hh"
#include "dwarf-explorer.hh"
#include "table-generator.hh"

std::unordered_map<std::string, std::list<std::string>> artificial_features;

void parse_parameters(const int argc, const char **argv, unsigned int & max_depth, unsigned int & max_features, 
			std::unordered_set<std::string> & cu_bl,
			std::unordered_set<std::string> & cu_wl,
			std::unordered_set<std::string> & symbols_bl,
			std::unordered_set<std::string> & symbols_wl
			) {
	for (int a = 2; a < argc; a++) {
		if (strncmp(argv[a], "--max-depth=", 12) == 0) {
			int d = atoi(argv[a] + 12);
			if (d == 0) {
				utils::log(VL_ERROR, "Error parsing max-depth, or max-depth == 0!");
				exit(-1);
			}
			max_depth = d;
		} else if (strncmp(argv[a], "--max-features=", 15) == 0) { 
			int f = atoi(argv[a] + 15);
			if (f == 0) {
				utils::log(VL_ERROR, "Error parsing max-features, or max-features == 0!");
				exit(-1);
			}
			max_features = f;
		} else if (strncmp(argv[a], "--sym_wl=", 9) == 0) {
			// TODO: only one symbol at a time is supported right now
			std::string sym_name = argv[a];
			sym_name = sym_name.substr(9, std::string::npos);
			int comma_pos = -1;
			int last_idx;
			do {
				last_idx = comma_pos + 1;
				comma_pos = sym_name.find(",", last_idx);
				std::string sn = sym_name.substr(last_idx, comma_pos - last_idx);
				symbols_wl.insert(sn);
				utils::log(VL_DEBUG, "Adding sym " + sn);
			}
			while (comma_pos != std::string::npos);
		} else if (strncmp(argv[a], "--cu_wl=", 8) == 0) {
			utils::log(VL_ERROR, "TODO: implement " + std::string(argv[a]));
		} else {
			utils::log(VL_ERROR, "Unhandled parameter: " + std::string(argv[a]));
			exit(-1);
		}

	}
}

int main(const int argc, const char **argv)
{
        if (argc < 2) {
                fprintf(stderr, "usage: %s elf-file [--max-depth=n] [--sym_wl=path] [--cu_wl=path]\n", argv[0]);
                return 2;
        }

	unsigned int max_depth = MAX_DEPTH;
	unsigned int max_features = MAX_FEATURES;
	// BLACKLISTS FOR COMPILATION_UNITS AND SYMBOLS. IGNORED IF EMPTY.
	std::unordered_set<std::string> cu_bl = {};
	std::unordered_set<std::string> symbols_bl = {}; 
	// WHITELISTS FOR COMPILATION_UNITS AND SYMBOLS. IGNORED IF EMPTY.
	std::unordered_set<std::string> cu_wl = { };
	std::unordered_set<std::string> symbols_wl = { }; 

	// Parse parameters
	parse_parameters(argc, argv, max_depth, max_features, cu_bl, cu_wl, symbols_bl, symbols_wl);

	// Load the binary
        int fd = open(argv[1], O_RDONLY);
        if (fd < 0) {
                fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
                return 1;
        }
        elf::elf ef(elf::create_mmap_loader(fd));
        dwarf::dwarf dw(dwarf::elf::create_loader(ef));

	// FIXME: dwarf_explorer still wants the binary filename to find definitions when needed
	dwarf_explorer * curiosity = new dwarf_explorer(argv[1], max_depth, max_features);

	// PHASE 1: explore dwarf info to find function and variables
        for (auto cu : dw.compilation_units()) {
		const dwarf::die curoot = cu.root();
		std::string cuname = to_string(curoot[dwarf::DW_AT::name]);
		if ((cu_wl.size() > 0 && cu_wl.find(cuname) == cu_wl.end()) 
			|| (cu_bl.size() > 0 && cu_bl.find(cuname) != cu_bl.end()))
			continue;

		dwarf::line_table lt = cu.get_line_table();
		utils::log(VL_INFO, "### Exploring Compilation Unit " + cuname + "###");
#ifdef WITH_GLOBAL_VARIABLES
		curiosity->walk_tree_dfs(curoot, curoot, lt, curoot, symbols_wl, symbols_bl);
#else
		curiosity->walk_tree_dfs(curoot, curoot, lt);
#endif
        }
	utils::log(VL_INFO, "### DONE EXPLORING DWARF INFO ###");

	// PHASE 2: generate code and table info
	if (curiosity->found_info()) {
		utils::log(VL_INFO, "Creating table...");
		table_generator::create_table(curiosity, "table.txt");
		utils::log(VL_INFO, "Table created!");
		utils::log(VL_INFO, "Creating instrumentation code...");
		code_generator::create_instrumentation_code(curiosity, "feature_processing.cc");
		utils::log(VL_INFO, "Instrumentation code created!");
	} else {
		utils::log(VL_ERROR, "Could not find info for the given symbols!");
	}

	utils::log(VL_INFO, "All done!");
	delete curiosity;
	return 0;
}
