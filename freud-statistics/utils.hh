#ifndef UTILS_HH_INCLUDED
#define UTILS_HH_INCLUDED

#include "method.hh"

enum verbosity_levels {
	VL_ERROR = 0,
	VL_QUIET,
	VL_INFO,
	VL_DEBUG
};


class utils {
private:
	static std::string log_label(enum verbosity_levels vl);

public:
	static void log(enum verbosity_levels l, std::string msg);
	static bool similar_cwc(struct corr_with_constraints cwc1, struct corr_with_constraints cwc2);

	static bool similar_regressions(std::vector<struct corr_with_constraints> vr1, std::vector<struct corr_with_constraints> vr2);

	static void get_filtered_data(corr_with_constraints cwc, const std::vector<measure *> in, std::vector<measure *> &out);	
	
	static void get_main_trend(std::string fkey, metric_type metric, const std::vector<measure *> data, std::vector<measure *> &res);

	static void remove_addnoise(std::string fkey, metric_type metric, const std::vector<measure *> data, std::vector<measure *> &res);

	static std::string get_random_color(int seed);
	static int get_degree(std::string feat);
	static std::string base_feature(std::string feature);
	
	// Annotation string generators
	static std::string generate_annotation_string(const struct annotation * pa);
	static std::string generate_annotation_string(const corr_with_constraints cwc);
	static std::string generate_annotation_string(const std::vector<corr_with_constraints> * in);
	static std::string generate_annotation_string(const std::vector<struct cluster> * clusters);
	static bool comp_contains(const std::string &comp, const std::string &f);
};

#endif
