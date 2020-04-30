#ifndef MEASURE_HH_INCLUDED
#define MEASURE_HH_INCLUDED

class measure {
private:
public:
	measure() {};
	measure(std::string mname) {};
	// I probably want to add a method here, either by name or by struct method
	std::string method_name;
	unsigned char depth;
	uint64_t measures[METRICS_COUNT];
	std::unordered_map<uint64_t, std::vector<bool>> branches;
	std::vector<struct feature> features;
	measure *parent;
	//std::vector<measure *> children;
	std::vector<uint64_t> children_ids;
	
	
	static unsigned char get_feature_idx(std::string fkey) {
		int pipe_pos = fkey.find("|");
		unsigned char idx = (unsigned char)(atoi(fkey.substr(0, pipe_pos).c_str()));
		return idx;
	}
	static feature_type get_feature_type(std::string fkey) {
		int pipe_pos = fkey.find("|");
		feature_type ftype = (feature_type)(atoi(fkey.substr(pipe_pos+1, fkey.length()-pipe_pos).c_str()));
		return ftype;
	}

	std::string get_v_name(std::string fkey) {
		int pipe_pos = fkey.find("|");
		int idx  = atoi(fkey.substr(0, pipe_pos).c_str());
		feature_type ftype = (feature_type)(atoi(fkey.substr(pipe_pos+1, fkey.length()-pipe_pos).c_str()));
		for (feature f: features) {
			if (f.idx == idx && ftype == f.type)
				return f.var_name;
		}
		return "fixme_notfound";
	}

	long long int get_feature_value(std::string fkey) {
		int colon_pos = fkey.find(":");
		if (colon_pos == std::string::npos) {
			int pipe_pos = fkey.find("|");
			int idx  = atoi(fkey.substr(0, pipe_pos).c_str());
			feature_type ftype = (feature_type)(atoi(fkey.substr(pipe_pos+1, fkey.length()-pipe_pos).c_str()));
			for (feature f: features) {
				if (f.idx == idx && ftype == f.type)
					return f.value;
			}
		} else {
			printf("THIS SHOULD NOT HAPPEN! OFFENDING FKEY: %s\n", fkey.c_str());
			exit(-1);
			/*
			std::string f1, f2;
			f1 = fkey.substr(0, colon_pos);
			f2 = fkey.substr(colon_pos+1);
			return get_feature_value(f1) * get_feature_value(f2);
			*/
		}
		return 0;
	}

};

#endif
