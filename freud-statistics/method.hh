#ifndef MEASURE_HH_INCLUDED
#define MEASURE_HH_INCLUDED

#include <vector>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <stdint.h>
#include <string.h>
#include <sstream>
#include <limits>

#include "const.hh"
#include <climits>
#include <iostream>
#include <fstream>

#include "regression.hh"

struct cluster {
	double lower_bound;
	double upper_bound;
	double centroid;
	double probability;
	double variance;
	cluster() {};
	cluster(std::stringstream &ss) {
		std::string tmp;
		ss >> tmp; // CL[
		ss >> probability;
		ss >> tmp; // ]:
		ss >> tmp; // B(	
		// lower_b can be -Inf!
		ss >> tmp;
		if (tmp != "-inf")
			lower_bound = std::stod(tmp, 0);
		else
			lower_bound = -std::numeric_limits<double>::infinity();
		ss >> tmp; // ","
		ss >> tmp;
		if (tmp != "inf")
			upper_bound = std::stod(tmp, 0);
		else
			upper_bound = std::numeric_limits<double>::infinity();
		//std::cout << "Found UB: " << upper_b << std::endl;
		ss >> tmp; // "),"
		ss >> tmp; // "C("
		ss >> centroid;
		ss >> tmp; // ")"
	};

	std::string get_string() const {
		return "CL[ " + std::to_string(probability) + " ]: B( " + std::to_string(lower_bound) + " , " + std::to_string(upper_bound) + " ), C( " + std::to_string(centroid) + " )\n";  
	}; 
};

struct path_filter {
	path_filter(): feature(""), lower_bound(0), upper_bound(0) {};
	path_filter(std::string ft, filter_type fl, int lb, int ub): feature(ft), flt(fl), lower_bound(lb), upper_bound(ub) {};
	std::string feature;
	filter_type flt;
	int lower_bound;
	int upper_bound;
	double probability;
	double cl_lower_bound;
	double cl_upper_bound;
	std::string to_string() const {
		return feature + std::to_string(lower_bound) + std::to_string(upper_bound);
	}
};

struct feature {
	feature_type type;
	int64_t value;
};

struct on_stack_corr {
	std::string m1;
	std::string m2;
	std::string fkey_1;
	std::string fkey_2;

	bool operator==(const on_stack_corr &other) const { 
		return (m1 == other.m1
            && m2 == other.m2
            && fkey_1 == other.fkey_1
            && fkey_2 == other.fkey_2);
  	};
};

namespace std {
template <> struct hash<on_stack_corr> {
    	std::size_t operator()(const on_stack_corr& k) const {
		  using std::hash;
		  using std::string;

		  // Compute individual hash values for first,
		  // second and third and combine them using XOR
		  // and bit shifting:

		  return ((((hash<string>()(k.m1)
				   ^ (hash<string>()(k.m2) << 1)) >> 1)
				   ^ (hash<string>()(k.fkey_1) << 1)) >> 1)
				   ^ (hash<string>()(k.fkey_2) << 1)
			  ;
		}
	};
}

struct branch_corr {
	std::string feature;
	uint16_t baddr;
	std::vector<int> pivots;
};

class measure {
private:
public:
	measure() {};
	measure(std::string mname) {};
	// I probably want to add a method here, either by name or by struct method
	std::string method_name;
	uint64_t measures[METRICS_COUNT];
	std::unordered_map<uint64_t, std::vector<bool>> branches;
	std::unordered_map<std::string, struct feature> features_map;

	// STACK CORR
	measure *parent;
	std::vector<uint64_t> children_ids;
	
	feature_type get_feature_type(std::string fkey) {
		if (features_map.find(fkey) == features_map.end())
			return FT_UNKNOWN;
		return features_map.at(fkey).type;
	}

	int64_t get_feature_value(std::string fkey) const {
		int colon_pos = fkey.find(":");
		if (colon_pos == std::string::npos) {
			if (features_map.find(fkey) == features_map.end())
				return 0;
			return features_map.at(fkey).value;
		} else {
			printf("THIS SHOULD NOT HAPPEN! OFFENDING FKEY: %s\n", fkey.c_str());
			exit(-1);
		}
		return 0;
	}

};

struct corr_with_constraints {
	std::string path_conditions;
	std::vector<struct path_filter> path_conditions_vec;
	bool constant;
	int constant_value;
	multiple_regression * reg;
	bool addnoise_removed;
	bool main_trend;
	metric_type metric;
	std::vector<measure *> data_points;
	corr_with_constraints() {};
	
	// parse from string constructor
	corr_with_constraints(std::stringstream &ss) {
		//std::cout << "PCWC " << in << std::endl;
		// TODO: what to do with this?
		constant = false;
		std::string temp;

		// TYPE OF REGRESSION {C, M, *}
		ss >> temp;
		addnoise_removed = (temp == "*");
		main_trend = (temp == "M");
	
		// PATH CONDITIONS	
		ss >> temp; // "["

		std::string pc_string = "";
		// parse path condition here
		ss >> temp;
		while (temp != "]") {
read_condition:
			std::string feature, op;
			double value, value_2, value_3;

			feature = temp;
			//std::cout << "Feature " << feature << std::endl;
			if (feature == "*") {
				// No path vec
				ss >> temp; // "]"
				break;
			} else if (feature == "]") {
				// This should not happen
				std::cout << "unexpected ] found while parsing path_conditions" << std::endl;
				exit(-1);
			}

			ss >> op;
			//std::cout << "Op " << op << std::endl;
			ss >> value;
			struct path_filter pf;
			if (feature == "CL" && op == "==") {
				ss >> temp;
				if (temp != "inf")
					value_2 = std::strtod(temp.c_str(), 0);
				else
					value_2 = std::numeric_limits<double>::infinity();
				ss >> temp;
				if (temp != "inf")
					value_3 = std::strtod(temp.c_str(), 0);
				else
					value_3 = std::numeric_limits<double>::infinity();
				pf.probability = value;
				pf.cl_lower_bound = value_2;	
				pf.cl_upper_bound = value_3;	
			}
			else if (op == ">|") {
				ss >> temp; // "|"
				ss >> temp; // "<="
				ss >> value_2;
			} 
			//std::cout << "Val " << value << std::endl;
			pf.feature = feature;
			if (op == "<=") {
				pf.flt = FLT_LE; 
				pf.upper_bound = value;
			} else if (op == ">") {
				pf.flt = FLT_GT; 
				pf.lower_bound = value;
			} else if (op == "==") {
				pf.flt = FLT_EQ; 
				pf.upper_bound = pf.lower_bound = value;
			} else if (op == "!=") {
				pf.flt = FLT_DF; 
				pf.upper_bound = pf.lower_bound = value;
			} else if (op == ">|") {
				pf.flt = FLT_BT; 
				pf.lower_bound = value;
				pf.upper_bound = value_2;
			} else {
				pf.flt = FLT_NONE;
			} 
			path_conditions_vec.push_back(pf);
			ss >> temp;
			//std::cout << "Temp " << temp << std::endl;
			if (temp == ",") {
				// Another PC here
				//std::cout << "Multiple path conditions unhandled, quitting!" << std::endl;
				ss >> temp; // this should be the next feature
				goto read_condition;
				//exit(-1);
			}
		}
		ss >> temp;
		if (temp == "T")
			metric = MT_TIME;
		else if (temp == "M")
			metric = MT_MEM;
		else if (temp == "L")
			metric = MT_LOCK;
		else if (temp == "W")
			metric = MT_WAIT;
		else if (temp == "p")
			metric = MT_PAGEFAULT_MINOR;
		else if (temp == "P")
			metric = MT_PAGEFAULT_MAJOR;

		ss >> temp; // white space

		// THE ACTUAL REGRESSION
		reg = new multiple_regression(ss);
	}
};

struct annotation {
	std::vector<corr_with_constraints> regressions;
	std::vector<cluster> clusters;
	annotation() {};	
	annotation(const std::string &filename) {
		std::ifstream in(filename.c_str(), std::ios::in);
		
		if (!in.is_open()) {
			std::cout << "Cannot open: " << filename << std::endl;
			return;
		}

		std::string temp;
		std::string type;
		in >> type;
		while (in) {
			std::string line;
			getline(in, line);
			if (type == "R") {
				// read one line
				std::stringstream ssl(line);
				regressions.push_back(corr_with_constraints(ssl));
			} else if (type == "CL[") {
				std::stringstream ssl(line);
				clusters.push_back(cluster(ssl));	
			} else {
				// This should not happen
				std::cout << "Unexpected token: " << type << std::endl;
				exit(-1);
			}
			in >> type;
		}
		in.close();
	};
};

struct method {
	std::string name;
	std::vector<measure *> original_data;
	std::vector<measure *> data;
	std::unordered_map<std::string, std::vector<measure *>> no_addnoise_data;
	std::unordered_set<std::string> feature_set;
	std::unordered_set<uint64_t> all_single_branches;
	std::unordered_set<uint64_t> all_branches;
	struct annotation performance_annotation;
	std::unordered_set<std::string> enums;
	
	std::vector<struct branch_corr> get_enum_partitions() {
		// std::unordered_set<std::string> enums;
		//struct branch_corr {
		//	std::string feature;
		//	uint16_t baddr;
		//	std::vector<int> pivots;
		//};
		std::vector<struct branch_corr> res;
		for (std::string e: enums) {
			std::set<int> possible_values;
			for (measure * m: data) {
				int f_value = m->get_feature_value(e);
				possible_values.insert(f_value);
			}
			struct branch_corr bc;
			bc.feature = e;
			bc.baddr = 0;
			for (int p: possible_values) {
				bc.pivots.push_back(p);
			}
			res.push_back(bc);
		}
		return res;
	}

	void add_loops_as_feature() {
		// I need to find all the possible branches first, so that the missing
		// ones have a count of zero
		uint16_t idx = 0;
		std::unordered_map<uint64_t, uint16_t> id_to_idx;
		for (measure * m: data) {
			for (std::pair<uint64_t, std::vector<bool>> b: m->branches) {
				all_branches.insert(b.first);
				if (id_to_idx.find(b.first) == id_to_idx.end()) {
					id_to_idx.insert(std::pair<uint64_t, uint16_t>(b.first, idx++));
				}
			}
		}
		int base_idx = feature_set.size();
		for (measure * m: data) {
			//std::cout << "##### newm ##### " << std::endl;
			std::unordered_set<uint64_t> local_b;
			for (std::pair<uint64_t, std::vector<bool>> b: m->branches) {
				int true_cnt = 0;
				for (bool eval: b.second)
					if (eval)
						true_cnt++;
				struct feature f;
				idx = id_to_idx[b.first];
				std::string var_name = "br" + std::to_string(b.first);
				f.type = FT_BRANCH_ISTRUE;
				f.value = true_cnt; //b.second.size();
				m->features_map.insert(make_pair(var_name, f));
				//std::cout << "Added f " << f.var_name << " with " << f.value  << " and idx " << idx << std::endl;
				local_b.insert(b.first);
				feature_set.insert(var_name);
			}

			// Add the other branches, which are taken 0 times
			for (uint64_t b: all_branches) {
				if (local_b.find(b) == local_b.end()) {
					struct feature f;
					idx = id_to_idx[b];
					std::string var_name = "br" + std::to_string(b);
					f.type = FT_BRANCH_ISTRUE;
					f.value = 0;
					m->features_map.insert(make_pair(var_name, f));
					//std::cout << "Added f " << f.var_name << " with " << f.value << std::endl;
					feature_set.insert(var_name);
				}
			}
		}
	}

	std::vector<struct branch_corr> find_correlated_branches(std::string fkey) {
		std::vector<struct branch_corr> res_vector; 	
		if (all_single_branches.size() == 0) {
			// populate the set of all the branches that occurr in the method
			for (measure * m: data) {
				for (std::pair<uint64_t, std::vector<bool>> b: m->branches) {
					if (b.second.size() == 1)
						all_single_branches.insert(b.first);
// TODO: want to extend this?
				}
			}
		}
		for (uint64_t branch: all_single_branches) {
			int min_t = INT_MAX, max_t = INT_MIN, min_n = INT_MAX, max_n = INT_MIN;
			bool at_least_one_t = false, at_least_one_n = false, skip = false; 
			//std::cout << "Analyzing branch " << branch << " against " << data.size() << " samples" << std::endl;
			for (measure * m: data) {
				if (m->branches.find(branch) == m->branches.end()) {
					//std::cout << "Cannot find branch " << branch << std::endl;
					skip = true;
					break;	
				}
				bool taken = m->branches[branch][0]; // I know this vector has 1! element
				int f_value = m->get_feature_value(fkey);
				if (taken) {
					if (f_value < min_t)
						min_t = f_value;
					if (f_value > max_t)
						max_t = f_value;
					at_least_one_t = true;
				} else {
					if (f_value < min_n)
						min_n = f_value;
					if (f_value > max_n)
						max_n = f_value;
					at_least_one_n = true;
				}
			}
			//std::cout << fkey << "; minT: " << min_t << ", maxT: " << max_t << ", minN: " << min_n << ", maxN: " << max_n <<  "; skip: " << skip << std::endl;
			if (!at_least_one_t || !at_least_one_n || skip)
				continue;
			struct branch_corr res;
			if (max_n < min_t) {
				if (min_n == max_n)
					res.pivots.push_back(min_n);
				else if (min_t == max_t)
					res.pivots.push_back(max_t);
				else 
					res.pivots.push_back(max_n);
			} else if (max_t < min_n) {
				if (min_t == max_t)
					res.pivots.push_back(max_t);
				else if (min_n == max_n)
					res.pivots.push_back(min_n);
				else 
					res.pivots.push_back(max_t);
			} else {
				continue; // no partitioning
			}
			std::cout << "Found part " << branch << " wfeat: " << fkey << std::endl;
			res.baddr = branch; 
			res.feature = fkey;
			res_vector.push_back(res);
		}
		return res_vector;
	}

	std::vector<struct branch_corr> find_correlated_branches() {
		std::vector<struct branch_corr> res;
		for (std::string f: feature_set) {
			// enums are considered independently, so skip here...
			if (enums.find(f) != enums.end())
				continue;

			for (struct branch_corr bc: find_correlated_branches(f))
				res.push_back(bc);
		}
		return res;
	}
};

#endif
