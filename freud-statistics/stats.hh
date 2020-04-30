#ifndef STATS_HH_INCLUDED
#define STATS_HH_INCLUDED

#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "method.hh"
#include "regression.hh"

// Maximum allowed correlation between two different features when filtering
#define CORRELATION_THRESHOLD 0.9

// Minimum r2 value that is accepted for a correlation
#define MIN_DET 0.5

// Maximum acceptable p-value for a feature in a regression
#define GOOD_PVALUE 2e-11 

// Additional penalty for a higher degree for a regression
#define BIC_DIFF 10

// When filtering features to have a better understanding of the main features,
// how many features to remove at each iteration
#define DROP_K 5

// Degree used to represent internally the nlogn term
#define LOG_DEG 3 

// Constants used to evaluate the distance between performance annotations
#define MAX_CONST_DST 1.05
#define MAX_CLUST_DST 0.05

class stats {
private:
	static void compute_tot_min_max(const std::vector<measure *> & data, std::string fkey, metric_type mtype, double &tot, double &sqr_tot, double &min, double &max, double &tot_feature, double &sqr_tot_feature, bool remove_dependencies);
	static double compute_correlation_coefficient(const std::vector<measure *> & data, std::string fkey, metric_type mtype, bool spearman);
	static void compute_variances(const std::vector<measure *> & data, double tot, double sqr_tot, double tot_feature, double sqr_tot_feature, double &var, double &feature_var, const double avoid_c_cancellation_metric, const double avoid_c_cancellation_feature, const std::string fkey, metric_type mtype);

public:
	/***********************
	  REGRESSION ANALYSIS
	**********************/
	static void find_noncorr_features(const std::vector<measure *> &in_data, const std::unordered_set<std::string> &features, std::vector<std::string> &noncorr_features);
	static void free_r_memory(const int howmany);
	static void * prepare_data_for_r(const std::vector<measure *> &data, const std::vector<std::string> &features, std::vector<std::string> &features_interaction, std::vector<std::string> &quad_features, std::vector<std::string> &quad_features_interaction, std::vector<std::string> &log_features, std::vector<std::string> &log_features_interaction, std::unordered_map<std::string, std::string> &names_map_to_r, std::unordered_map<std::string, std::string> &names_map_to_c, std::unordered_map<std::string, std::string> &quad_names_map_to_r, std::unordered_map<std::string, std::string> &quad_names_map_to_c, std::unordered_map<std::string, std::string> &log_names_map_to_r, std::unordered_map<std::string, std::string> &log_names_map_to_c, const metric_type mtype);
	static bool compute_multiple_regression(const std::vector<measure *> &data, const std::vector<std::string> &features_to_use, const std::vector<std::string> &quad_features_to_use, const std::vector<std::string> &log_features_to_use, const std::vector<std::string> &features_to_use_interaction, const std::vector<std::string> &quad_features_to_use_interaction, const std::vector<std::string> &log_features_to_use_interaction, metric_type mtype, std::vector<std::string> &used_features, double &det, double &intercept, std::vector<std::vector<double>> &coefficients, double &bic, const int degree, void * matp, const std::unordered_map<std::string, std::string> &names_map_to_r, const std::unordered_map<std::string, std::string> &names_map_to_c, const std::unordered_map<std::string, std::string> &quad_names_map_to_r, const std::unordered_map<std::string, std::string> &quad_names_map_to_c, const std::unordered_map<std::string, std::string> &log_names_map_to_r, const std::unordered_map<std::string, std::string> &log_names_map_to_c, std::unordered_map<std::string, double> &coefficients_map, const bool refining, const double min_det);

	/***********************
	  CLUSTER ANALYSIS
	**********************/
	static void kde_clustering(const std::vector<measure *> & data, const metric_type mtype, std::vector<struct cluster> & clusters, bool R_needs_init);
	static void create_clusters(const std::vector<measure *> & data, const metric_type mtype, const double * kde, const double min, const double max, std::vector<struct cluster> & clusters);
	
	/***********************
	  MISC 
	**********************/
	static void is_constant(const std::vector<measure *> & data, const metric_type mtype, bool &is_c, uint64_t &c);

	/***********************
	  PERFORMANCE ANNOTATION VALIDATION 
	**********************/
	static bool validate_cluster_before_regression(const std::vector<measure *> & data, metric_type mtype, const double cl_probability, const double lb, const double ub, std::vector<measure *> & f_data);
	static bool validate_regression(const std::vector<measure *> & data, std::string feature_key, metric_type mtype, multiple_regression * r);
	static bool validate_constant(const std::vector<measure *> & data, metric_type mtype, int cvalue);
	static bool validate_clusters(const std::vector<measure *> & data, metric_type mtype, const std::vector<struct cluster> & clusters);

	/***********************
	  PLOTTING 
	**********************/
	static void printGNUPlotData(const std::vector<measure *> & data, std::vector<std::string> fkeys, const metric_type mtype, bool remove_dependencies, std::string &res, double min, double max);
	static void get_ofeature_intervals(const std::vector<measure *> & data, std::string feat, std::vector<double> &values, const int intervals);
	static double get_avg_value(const std::vector<measure *> & data, std::string feat);
};

#endif
