#include "reader.hh"

void bin_reader::read(std::string filename, std::unordered_map<std::string, struct routine_descriptor> &rmap) {
	ifstream table(filename.c_str());
	std::string line, name, fhash;
	int count, isptr, fcount, isaddr, fsets;
	uint64_t address, pos;
	int64_t off;
	int lnum = 0;
	while (std::getline(table, line)) {
		lnum++;
		if (line != "###") {
			std::ostringstream oss;
			oss << "Parsing problem at line " << lnum;
			log(VL_ERROR, oss.str());
			oss.str("");
			oss << "Expected ###, got " << line;
			log(VL_ERROR, oss.str());
			exit(-1);
		}
		// SYMBOL NAME
		std::getline(table, name);
		lnum++;

		// INSTR ENTRY POINT
		std::getline(table, line);
		lnum++;
		address = strtoull(line.c_str(), NULL, 10);

		// NUM OF PARAMS
		std::getline(table, line);
		lnum++;
		count = atoi(line.c_str());
		rmap[name].name = name;
		rmap[name].address = address;
		rmap[name].taken_count = 0;
		rmap[name].param_count = (uint16_t)count;
		
		for (int i = 0; i < count; i++) {
			// LOCATION
			std::getline(table, line);
			lnum++;
			pos = strtoull(line.c_str(), NULL, 10);

			// OFFSET FROM LOCATION
			std::getline(table, line);
			lnum++;
			off = strtoll(line.c_str(), NULL, 10);

			// IS_ADDR | IS_REG_WITH_ADDR
			std::getline(table, line);
			lnum++;
			isaddr = atoi(line.c_str());

			// IS_PTR
			std::getline(table, line);
			lnum++;
			isptr = atoi(line.c_str());

			struct dwarf_formal_parameter dfp;	
			// TYPE_NAME
			std::getline(table, line);
			dfp.type_name = line;
			lnum++;

			// NUMBER OF FEATURE_SETS
			std::getline(table, line);
			lnum++;
			fsets = atoi(line.c_str());

			int max_f_c = 0;
			for (int f = 0; f < fsets; f++) {
				// HASH OF THE FEATURE SET
				std::getline(table, fhash);
				lnum++;

				// NUM_OF_FEATURES
				std::getline(table, line);
				lnum++;
				fcount = atoi(line.c_str());
				if (fcount > max_f_c)
					max_f_c = fcount;
					
				log(VL_DEBUG, "Found runtime type " + fhash + " for " + dfp.type_name); 

				for (int c = 0; c < fcount; c++) {
					// TYPE_NAME
					std::getline(table, line);
					lnum++;
					string ht = line;

					// FEATURE_NAME
					std::getline(table, line);
					lnum++;
					string ft = line;
					dfp.runtime_type_to_features[fhash].push_back(primitive_feature(ht, ft));
					vector<uint32_t> ca;
					uint32_t dims = 0;
					if (ft.find("array ") == 0) {
						// read info about the dims
						std::getline(table, line);
						lnum++;
						dims = strtoul(line.c_str(), 0, 10);
						for (unsigned int i = 0; i < dims; i++) {
							std::getline(table, line);
							lnum++;
							uint32_t c = atoi(line.c_str());
							ca.push_back(c);
						}
					}
					// TODO: DO SOMETHING!
					//dfs.array_dims.push_back(dims);		
					//dfs.array_counts.push_back(ca);
				}
			}
			dfp.position		= pos;
			dfp.offset		= off;
			dfp.is_addr		= ((bool)(isaddr > 0));
			dfp.is_reg_with_addr	= ((bool)(isaddr > 1));
			dfp.is_ptr		= ((bool)isptr);
			rmap[name].params.push_back(dfp);
			rmap[name].max_features_count += max_f_c;
		}

		// Add system features
		// I have only 1! sys feature right now, hardcoded in rtn_descriptor
		rmap[name].system_variable_names.push_back("cpu_clock");
	}
	
}

