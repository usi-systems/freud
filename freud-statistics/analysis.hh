#ifndef ANALYSIS_HH_DEFINED
#define ANALYSIS_HH_DEFINED

#include <vector>
#include <map>
//#include <string.h>
#include <iostream>

#include "stats.hh"
#include "method.hh"
#include "utils.hh"

#define PCC_THRESHOLD 0.4

enum phase {
	ORIGINAL = 0,
	REMOVED_NOISE,
	MAIN_TREND
};

class analysis {
private:
	// data set
	//const std::vector<measure> * data;
	std::unordered_map<uint64_t, measure *> * m_ids_map;
	method * mtd;
	metric_type metric;
	feature_type ftype;
	double min_det; 
	
	bool compute_best_multiple_regression(std::vector<corr_with_constraints> &result);
	bool explore_regression_tree(const std::vector<measure *> & data, std::vector<struct branch_corr> branches, std::vector<corr_with_constraints> &result, int depth, const std::vector<struct path_filter> & path_conditions, const std::unordered_set<std::string> features, const enum phase ph);
	multiple_regression * compute_best_multiple_regression(const std::vector<measure *> &in_data, const std::unordered_set<std::string> &features);

	bool split_on_pivots(const std::vector<int> pvalues, const std::string fkey, const std::vector<measure *> in, std::vector<std::vector<measure *>> &out);

public:
	analysis(const std::string &mname, method * m, metric_type mtype, std::unordered_map<uint64_t, measure *> * mim);
	bool cluster();
	bool cluster(std::vector<struct cluster> &result);
	bool find_regressions();
	bool find_regressions(double min_r2);
};

#endif
