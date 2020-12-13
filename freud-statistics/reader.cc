#include "reader.hh"
#define MAX_FEATURES 512 

feature_type reader::get_type(const std::string & t) {
	if (t.find("enum ") == 0) {
		return FT_ENUM;
	} else if (t.find("int") != std::string::npos
		|| t.find("long") != std::string::npos
		|| t.find("float") != std::string::npos
		|| t.find("double") != std::string::npos
		) {
		return FT_INT;
	} else if (t.find("char") != std::string::npos) {
		return FT_STRING;
	} else if (t.find("size") != std::string::npos) {
		return FT_SIZE;
	} else if (t.find("array") != std::string::npos) {
		return FT_COLLECTION;
	}
	return FT_UNKNOWN;
}

void reader::read_file_binary(std::string filename, std::map<std::string, method *> &data, std::unordered_map<uint64_t, measure *> & m_ids_map, const uint32_t file_idx) {
	//std::cout << "Reading file " << filename << std::endl;
	std::ifstream in(filename.c_str(), std::ios::binary);
	
	if (!in.is_open())
		std::cout << "Cannot open: " << filename << std::endl; 

	uint64_t entry_num = 0;

	while (in) {
		uint32_t name_len;
		in.read((char *)&name_len, sizeof(uint32_t));
		if (in.eof())
			break;

		char *fname = new char[name_len + 1];
		in.read(fname, sizeof(char) * name_len);
		fname[name_len] = '\0';
		method * mtd;
		if (data.find(fname) == data.end()) {
			mtd = new method();
			data[fname] = mtd;
		}
		else {
			mtd = data.at(fname);
			mtd->name = fname;
		}
		//std::cout << "METHOD " << fname << ": " << mtd->data.size() << std::endl;

		// FEATURE NAMES
		uint32_t feature_names_count;
		in.read((char *)&feature_names_count, sizeof(uint32_t));
		std::unordered_map<uint64_t, std::string> feature_names;
		//std::cout << "fcount " << fcount << ", vncount " << vncount << std::endl;
		char *vname = new char[UINT16_MAX];
		for (uint32_t v = 0; v < feature_names_count; v++) {
			uint16_t vnlen;
			uint64_t foff = in.tellg();
			in.read((char *)&vnlen, sizeof(uint16_t));
			//std::cout << "LEN: " << vnlen << std::endl;
			in.read(vname, sizeof(char) * vnlen);
			vname[vnlen] = '\0';
			feature_names.insert(std::make_pair(foff, vname));
			//std::cout << "VAR: " << vname << " at " << foff << std::endl;
		}
		delete[] vname;

		// TYPE NAMES
		uint32_t type_names_count;
		in.read((char *)&type_names_count, sizeof(uint32_t));
		std::unordered_map<uint64_t, std::string> type_names;
		//std::cout << "fcount " << fcount << ", vncount " << vncount << std::endl;
		char *tname = new char[UINT16_MAX];
		for (uint32_t v = 0; v < type_names_count; v++) {
			uint16_t vnlen;
			uint64_t foff = in.tellg();
			in.read((char *)&vnlen, sizeof(uint16_t));
			//std::cout << "LEN: " << vnlen << std::endl;
			in.read(tname, sizeof(char) * vnlen);
			tname[vnlen] = '\0';
			type_names.insert(std::make_pair(foff, tname));
			//std::cout << "TYPE: " << tname << " at " << foff << std::endl;
		}
		delete[] tname;

		uint32_t samples_count;
		in.read((char *)&samples_count, sizeof(uint32_t));
		for (uint32_t s = 0; s < samples_count; s++) {
			uint64_t time, mem, lock_holding_time, waiting_time, minor_page_faults, major_page_faults;
			uint32_t uid_r;
			in.read((char *)&uid_r, sizeof(uint32_t));
			in.read((char *)&time, sizeof(uint64_t));
			in.read((char *)&mem, sizeof(uint64_t));
			in.read((char *)&lock_holding_time, sizeof(uint64_t));
			in.read((char *)&waiting_time, sizeof(uint64_t));
			in.read((char *)&minor_page_faults, sizeof(uint64_t));
			in.read((char *)&major_page_faults, sizeof(uint64_t));
			if (waiting_time > time || lock_holding_time > time) {
				std::cout << "Entry " << entry_num << std::endl;
				std::cout << "Time: " << time << std::endl;
				std::cout << "Mem: " << mem << std::endl;
				std::cout << "Lock: " << lock_holding_time << std::endl;
				std::cout << "Wait: " << waiting_time << std::endl;
				std::cout << "MiPF: " << minor_page_faults << std::endl;
				std::cout << "MaPF: " << major_page_faults << std::endl;
				exit(-1);
			}
			uint64_t uid = ((1llu << 32) * (const uint64_t)file_idx) + uid_r;
			measure *m = new measure();
			m->method_name = fname;
			if (m_ids_map.find(uid) != m_ids_map.end()) {
				std::cout << "Non unique id found! " << fname << "; " << uid << std::endl;
				std::cout << "Prev: " << m_ids_map.at(uid)->method_name << std::endl;
				exit(-1);
			}
			m_ids_map.insert(std::make_pair(uid, m));
			m->measures[MT_TIME] = time;
			m->measures[MT_MEM] = mem;
			m->measures[MT_LOCK] = lock_holding_time;
			m->measures[MT_WAIT] = waiting_time;
			m->measures[MT_PAGEFAULT_MINOR] = minor_page_faults;
			m->measures[MT_PAGEFAULT_MAJOR] = major_page_faults;

			// NUM OF FEATURES
			uint32_t num_of_features;
			in.read((char *)&num_of_features, sizeof(uint32_t));
			for (uint32_t f = 0; f < num_of_features; f++) {
				uint64_t name_offset;
				uint64_t type_offset;
				int64_t value;
				in.read((char *)&name_offset, sizeof(uint64_t));
				in.read((char *)&type_offset, sizeof(uint64_t));
				in.read((char *)&value, sizeof(int64_t));
				std::string type;
				if (type_offset > 0)
					type = type_names.at(type_offset);
				else
					type = "sysf"; // system feature
				struct feature feat;
				feat.type = get_type(type); // info not given by Pin
				std::string fname = feature_names.at(name_offset);
				if (feat.type == FT_ENUM) {
					mtd->enums.insert(fname);
				}
				// Never use the vptr or enums as an actual feature for regressions
				if (feat.type != FT_ENUM && fname.find("_vptr") == std::string::npos)
					mtd->feature_set.insert(fname);
				feat.value = value;
				//std::cout << "READ: " << fname << ": " << value << std::endl;
				m->features_map.insert(make_pair(fname, feat));
			}

			// BRANCHES
			uint32_t num_of_branches;
			in.read((char *)&num_of_branches, sizeof(uint32_t));
			for (int b = 0; b < num_of_branches; b++) {
				uint16_t branch_id;
				in.read((char *)&branch_id, sizeof(uint16_t));
				uint32_t num_of_executions;
				in.read((char *)&num_of_executions, sizeof(uint32_t));
				for (int e = 0; e < num_of_executions; e++) {
					bool taken;
					in.read((char *)&taken, sizeof(bool));
					m->branches[branch_id].push_back(taken);
				}
			}

			// CHILDREN
			uint32_t num_of_children;
			in.read((char *)&num_of_children, sizeof(uint32_t));
			for (int c = 0; c < num_of_children; c++) {
				uint32_t c_id;
				in.read((char *)&c_id, sizeof(uint32_t));
				uint64_t c_id_64 = (1lu << 32) * file_idx;
				c_id_64 += c_id;
				m->children_ids.push_back(c_id_64);
			}
			mtd->data.push_back(m);
		
			entry_num++;

		}
		delete[] fname;
		//std::cout << "Done reading samples" << std::endl;
	}
	in.close();
}


bool reader::read_folder(std::string folder_name, std::map<std::string, method *> &data, std::unordered_map<uint64_t, measure *> & m_ids_map) {
	DIR *dir;
	uint32_t fid = 0;
        struct dirent *ent;
        if ((dir = opendir(folder_name.c_str())) != 0) {
			while ((ent = readdir (dir)) != NULL) {
				if (strncmp(ent->d_name, "idcm", strlen("idcm")) == 0)
					read_file_binary(folder_name + ent->d_name, data, m_ids_map, fid++);
		}
		closedir(dir);
	} else {
			perror ("Could not open dir");
			return false;
        }
	//utils::log(VL_INFO, "Read " + std::to_string(m_ids_map.size()) + " measurements");
        return true;
}
