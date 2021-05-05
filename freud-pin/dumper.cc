size_t write_logs_to_file(std::string rtn_name, const struct routine_descriptor & desc) {
		time_t now = time(NULL);
		std::stringstream now_ss;
		now_ss.str("");
		mkdir("symbols/", S_IRWXU | S_IRWXG);
		std::string folder = "symbols/" + rtn_name;
		now_ss << folder << "/idcm_" << rtn_name << "_" << now << ".bin";
		size_t samples_count = 0;
		for (THREADID tid = 0; tid < MAXTHREADS; tid++) { 
			samples_count += desc.get_stored_history_size(tid); // I do not want to keep a counter in the struct, since I would need to sync on that
		}
		if (samples_count == 0) {
			return 0; // do not print unnecessary things
		}
		mkdir(folder.c_str(), S_IRWXU | S_IRWXG);
		ofstream outFile(now_ss.str().c_str(), std::ios::binary);
		if (!outFile.is_open()) {
			log(VL_ERROR, "Could not open outfile " + now_ss.str());
			return 0;
		}

		// **** RTN NAME ****
		uint32_t name_len = rtn_name.size();
		outFile.write((char *)&name_len, sizeof(uint32_t));
		outFile.write(rtn_name.c_str(), sizeof(char) * name_len);
			
		// **** FEATURES NAMES (incl. sys.) ****
		// print all the names of all the possible features
		// we will point to these strings afterwards
		unordered_map<string, uint64_t> fname_offsets;
		unordered_map<string, uint64_t> ftype_offsets;
		set<string> ftype_names;
		uint32_t tot_fnames = 0;
		std::fpos<mbstate_t> tot_fnames_position = outFile.tellp();
		outFile.write((char *)&tot_fnames, sizeof(uint32_t));
		
		for (auto p: desc.params) {
			for (auto rttf: p.runtime_type_to_features) {
				for (struct primitive_feature pf: rttf.second) {
					string fname = pf.name;
					if (fname_offsets.find(fname) == fname_offsets.end()) {
						fname_offsets.insert(make_pair(fname, outFile.tellp()));
						ftype_names.insert(pf.type);
						uint16_t fname_len = fname.size();
						outFile.write((char *)&fname_len, sizeof(uint16_t));
						outFile.write(fname.c_str(), sizeof(char) * fname_len);
						tot_fnames++;
					}
				}
			}
		}
		for (string sfname: desc.system_variable_names) {
			if (fname_offsets.find(sfname) == fname_offsets.end()) {
				fname_offsets.insert(make_pair(sfname, outFile.tellp()));
				uint16_t sfname_len = sfname.size();
				outFile.write((char *)&sfname_len, sizeof(uint16_t));
				outFile.write(sfname.c_str(), sizeof(char) * sfname_len);
				tot_fnames++;
			}
		}

		// **** TYPES NAMES ****
		uint32_t tcount = ftype_names.size();
		outFile.write((char *)&tcount, sizeof(uint32_t));
		for (string t: ftype_names) {
			ftype_offsets.insert(make_pair(t, outFile.tellp()));
			uint16_t ftype_len = t.size();
			outFile.write((char *)&ftype_len, sizeof(uint16_t));
			outFile.write(t.c_str(), sizeof(char) * ftype_len);
		}

		// GO BACK TO WRITE THE CORRECT NUM OF FEATURES
		std::fpos<mbstate_t> prev_pos = outFile.tellp();
		outFile.seekp(tot_fnames_position);
		outFile.write((char *)&tot_fnames, sizeof(uint32_t));
		outFile.seekp(prev_pos);
	
		// **** FOR EVERY RUN ****
		// I need to correct the number of samples, considering also the samples for which time == -1!
		// will do it later, though
		samples_count = 0;
		std::fpos<mbstate_t> num_of_samples_position = outFile.tellp();
		outFile.write((char *)&samples_count, sizeof(uint32_t));

		//cout << "Basics " << rtn.first << endl; 
		for (THREADID tid = 0; tid < MAXTHREADS; tid++) {
			if(desc.get_stored_history_size(tid) == 0)
				continue;
			//cout << "TID " << tid << endl; 
			
			const vector<struct rtn_execution *> * history = desc.get_stored_history_ptr(tid);
			for(uint32_t i = 0; i < history->size(); i++){
				uint64_t tim = (*history)[i]->diff();
				if (tim == UINT64_MAX)
					continue;
				//cout << "OBS " << i << ": " << (*history)[i]->total_waiting_time << endl; 
				
				samples_count++;

				// **** ID, [ METRICS ] ****
				outFile.write((char *)&(*history)[i]->unique_id, sizeof(uint32_t));
				outFile.write((char *)&tim, sizeof(uint64_t));
				outFile.write((char *)&(*history)[i]->allocated_memory, sizeof(uint64_t));
				outFile.write((char *)&(*history)[i]->total_lock_holding_time, sizeof(uint64_t));
				outFile.write((char *)&(*history)[i]->total_waiting_time, sizeof(uint64_t));
				outFile.write((char *)&(*history)[i]->minor_page_faults, sizeof(uint64_t));
				outFile.write((char *)&(*history)[i]->major_page_faults, sizeof(uint64_t));


				// **** NUM OF FEATURES ****
				uint32_t pn = 0, tot_features = 0;
				std::fpos<mbstate_t> tf_pos = outFile.tellp();
				outFile.write((char *)&tot_features, sizeof(uint32_t));
				uint32_t rot_idx = 0;	
				string runtime_type;
				// LOCAL AND GLOBAL FEATURES
				for (; pn < desc.param_count; pn++) {
					struct dwarf_formal_parameter dfp = desc.params[pn];
					if (dfp.runtime_type_to_features.size() > 1) {
						// No std::to_string() in STL...
						std::ostringstream oss;
						oss << (*history)[i]->runtime_types[rot_idx++];
						runtime_type = oss.str();
					} else if (dfp.type_name.find("class ") == 0 ||
						dfp.type_name.find("struct ") == 0) {
						std::ostringstream oss;
						string stmp = dfp.type_name.substr(dfp.type_name.find(" ") + 1, string::npos);
						oss << stmp.size();
						stmp = oss.str() + stmp;
						oss.str("");
						oss << freud_hash((const unsigned char *)stmp.c_str());
						runtime_type = oss.str();
					} else {
						// basic type
						runtime_type = "0";
					}

					if (dfp.runtime_type_to_features.find(runtime_type) == dfp.runtime_type_to_features.end()) {
						log(VL_ERROR, "Warning, found unknown dynamic type " + runtime_type + " for " + dfp.type_name);
						//exit(-1);
					} else {
						for (struct primitive_feature pf: dfp.runtime_type_to_features[runtime_type]) {
							if (tot_features >= (*history)[i]->feature_values.size()) {
								log(VL_ERROR, "Less features than expected in " + rtn_name + "!");
								log(VL_ERROR, "Type: " + runtime_type);
								std::ostringstream oss;
								oss.str("Got ");
								oss << (*history)[i]->feature_values.size();
								log(VL_ERROR, oss.str());
								oss.str("Feat ");
								oss << pf.name;
								log(VL_ERROR, oss.str());
								exit(-1);
							}
							uint64_t offs = fname_offsets[pf.name];
							uint64_t toffs = ftype_offsets[pf.type];
							outFile.write((char *)&offs, sizeof(uint64_t));
							outFile.write((char *)&toffs, sizeof(uint64_t));
							outFile.write((char *)&((*history)[i]->feature_values[tot_features++]), sizeof(int64_t));
						}
					}
				}
				if (tot_features != (*history)[i]->feature_values.size()) {
					log(VL_ERROR, "More features than expected in " + rtn_name + "!");
					log(VL_ERROR, runtime_type);
					std::ostringstream oss;
					oss.str("Exp ");
					oss << tot_features << " / Got " << (*history)[i]->feature_values.size();
					log(VL_ERROR, oss.str());
					exit(-1);
				}

				// SYSTEM FEATURES
				for (uint16_t f = 0; f < ((*history)[i])->system_feature_values.size(); f++) {
					// System features have 1! value by definition
					uint64_t offs = fname_offsets[desc.system_variable_names[f]];
					uint64_t toffs = 0; // Fake, it's a placeholder
					outFile.write((char *)&offs, sizeof(uint64_t));
					outFile.write((char *)&toffs, sizeof(uint64_t));
					outFile.write((char *)&((*history)[i]->system_feature_values[f]), sizeof(int64_t));
					tot_features++;
				}
				prev_pos = outFile.tellp();
				outFile.seekp(tf_pos);
				outFile.write((char *)&tot_features, sizeof(uint32_t));
				outFile.seekp(prev_pos);

				// BRANCHES
				uint32_t num_of_branches = (*history)[i]->branches.size();
				outFile.write((char *)&num_of_branches, sizeof(uint32_t));
				for (const std::pair<uint16_t, vector<bool>> &b: (*history)[i]->branches) {
					outFile.write((char *)&b.first, sizeof(uint16_t));
					uint32_t num_of_executions = b.second.size();
					outFile.write((char *)&num_of_executions, sizeof(uint32_t));
					for (bool t: b.second) {
						outFile.write((char *)&t, sizeof(bool));
					}
				}

				// CHILDREN
				uint32_t num_of_children = (*history)[i]->children.size();
				outFile.write((char *)&num_of_children, sizeof(uint32_t));
				for (uint32_t c: (*history)[i]->children)
					outFile.write((char *)&c, sizeof(uint32_t));
			}
		}
		prev_pos = outFile.tellp();
		outFile.seekp(num_of_samples_position);
		outFile.write((char *)&samples_count, sizeof(uint32_t));
		outFile.seekp(prev_pos);
		outFile.close();
		return samples_count;
}

VOID dump_logs(VOID * arg) {
	size_t tot_w = 0;
	while (!quit_dump_thread) {
		PIN_Sleep(DUMP_LOG_PERIOD);
		// SWITCH ALL DESCRIPTORS
		for (std::pair<std::string, struct routine_descriptor> ar: routines_catalog) {
			routines_catalog[ar.first].switch_history();
		}

		// DUMP
		for (std::pair<std::string, struct routine_descriptor> ar: routines_catalog) {
			// BINARY OUTPUT
			tot_w += write_logs_to_file(ar.first, ar.second);
		}
	};
	if (tot_w > 0) {
		std::ostringstream oss;
		oss << "Written info with " << tot_w << " samples";
		log(VL_INFO, oss.str());
	}
	else {
		log(VL_ERROR, "Nothing collected!");
	}
}
