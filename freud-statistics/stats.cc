#include "stats.hh"
#include <limits>
#include <cmath>
#include <Rinternals.h>
#include <Rembedded.h>

#include "utils.hh"

double stats::get_avg_value(const std::vector<measure *> & data, std::string feat) {
	double tot = 0;
	for (measure *m: data) 
		tot += m->get_feature_value(feat);
	tot /= data.size();
	return tot;
}

void stats::get_ofeature_intervals(const std::vector<measure *> & data, std::string feat, std::vector<double> &values, const int intervals) {
	double min = DBL_MIN, max = DBL_MAX;
	for (measure *m: data) {
		double val = m->get_feature_value(feat);
			if (val < max)
			max = val;
		if (val > min)
			min = val;	
	}
	double tmp = min;
	double span = (max - min) / (intervals - 1);
	for (int i = 0; i < intervals; i++) {
		values.push_back(tmp);
		tmp += span;
	}
}

void stats::printGNUPlotData(const std::vector<measure *> & data, std::vector<std::string> fkeys, const metric_type mtype, bool remove_dependencies, std::string &res, double min, double max) {
	min = -DBL_MAX;
	max = DBL_MAX;
	for (measure *m: data) {
		double mm = m->measures[mtype];
		for (std::string k: fkeys) {
			k = utils::base_feature(k);
			int colon_pos = k.find(":"); 
			if (colon_pos != std::string::npos) {
				continue;
			} else {
				double val = m->get_feature_value(k);
				if (val < max)
					max = val;
				if (val > min)
					min = val;	
				res.append(std::to_string(val));
				res.append(" ");
			}
		}
		res.append(std::to_string(mm));
		res.append("\n");
	}
}

void stats::is_constant(const std::vector<measure *> & data, const metric_type mtype, bool &is_c, uint64_t &c) {
	
	is_c = false;
	uint64_t min = std::numeric_limits<uint64_t>::max();
	uint64_t max = 0;
	double avg = 0;
	if (data.size() == 0)
		utils::log(VL_ERROR, "Data is empty in is_constant!");
	for (measure *m: data) {
		uint64_t mm = m->measures[mtype];
		avg += mm;
		if (mm < min)
			min = mm;
		if (mm > max)
			max = mm;
	}
	avg /= data.size(); // I know it is a positive num
	if ((avg == 0 && (max == min)) || (max - min) / avg < 0.01) {
		is_c = true;
		c = (max - min) / 2 + min;
	}
}

void stats::compute_tot_min_max(const std::vector<measure *> & data, const std::string fkey, const metric_type mtype, double &tot, double &sqr_tot, double &min, double &max, double &tot_feature, double &sqr_tot_feature, bool remove_dependencies) {

	min = DBL_MAX;
	max = 0;
	tot = sqr_tot = tot_feature = sqr_tot_feature = 0;

	// Compute totals, squares
	for (measure *m: data) {
		uint64_t mm = m->measures[mtype];
		if (mm < min)
			min = mm;
		if (mm > max)
			max = mm;

		tot += mm;
		sqr_tot += mm * mm;

		int fvalue = 0; 
		if (fkey != "")
			fvalue = m->get_feature_value(fkey);

		tot_feature += fvalue;	
		sqr_tot_feature += (long)fvalue * (long)fvalue;  
	}
}

void stats::compute_variances(const std::vector<measure *> & data, double tot, double sqr_tot, double tot_feature, double sqr_tot_feature, double &var, double &feature_var, const double avoid_c_cancellation_metric, const double avoid_c_cancellation_feature, const std::string fkey, metric_type mtype) {
	if (avoid_c_cancellation_metric != 0 || avoid_c_cancellation_feature) {
		tot = sqr_tot = tot_feature = sqr_tot_feature = 0;
		// recompute the values above
		for (measure *m: data) {
			double mm = m->measures[mtype] - avoid_c_cancellation_metric;
			double adjusted_min = mm; // + (1 + (double)m->children.size()) * instrumentation_cost[mtype] / 2;
			double adjusted_max = mm; // - (1 + (double)m->children.size()) * instrumentation_cost[mtype] / 2;

			tot += mm;
			sqr_tot += mm * mm;

			int fvalue = 0; 
			if (fkey != "")
				fvalue = m->get_feature_value(fkey) - avoid_c_cancellation_feature;

			tot_feature += fvalue;	
			sqr_tot_feature += (long)fvalue * (long)fvalue;  
		}
	}
	// Compute the mean and the variance (thus the squared mean as well)
	double avg = tot / data.size();
	var = (sqr_tot / data.size()) - (avg * avg);
	double feature_avg = tot_feature / data.size();
	feature_var = (sqr_tot_feature / data.size()) - (feature_avg * feature_avg);
		
	//std_dev = sqrt(var);
	//feature_std_dev = sqrt(feature_var);

//TODO: to be moved  to the analysis class
/*
	// STD deviation
	if (time_var == 0 || max_time <= min_time * IS_CONSTANT_THRESHOLD) {
		time_is_constant = true;
		// I use the avg as the expected value
		time_constant = (max_time + min_time) / 2;
	}
	// TODO: does it make sense to assume as constant a maximum memory difference of less than 5%?
	if (mem_var == 0 || max_memory <= min_memory * IS_CONSTANT_THRESHOLD) {
		mem_is_constant = true;
		memory_constant = (max_memory + min_memory) / 2;
	}
*/
}

double stats::compute_correlation_coefficient(const std::vector<measure *> & data, std::string fkey, metric_type mtype, bool spearman) {
	// Allocate an R vector and copy the C array into it.
	SEXP mat;
	PROTECT(mat = allocMatrix(INTSXP, data.size(),2));
	int *c_mat = INTEGER(mat); 
	for (int i = 0; i < data.size(); i++) {
		c_mat[i] = data[i]->get_feature_value(fkey);
		c_mat[data.size() + i] = data[i]->measures[mtype];
	}

	// Setup a call to the R function
	SEXP cc_call;
	PROTECT(cc_call = lang2(install("get_cc"), mat));
#if 0
	if (spearman == false)
		PROTECT(cc_call = lang2(install("get_pcc"), mat));
	else
		PROTECT(cc_call = lang2(install("get_scc"), mat));
#endif
	// Execute the function
	int errorOccurred;
	SEXP ret = R_tryEval(cc_call, R_GlobalEnv, &errorOccurred);

	double *val;
	double res = 0;
	if (!errorOccurred)
	{
		val = REAL(ret);
		res = val[0];
		//printf("R returned: %f\n", res);
	} else {
		printf("### R returned error: %d\n", errorOccurred);
	}
	// Unprotect add1_call and arg
	UNPROTECT(2);
	return res;
}

bool stats::validate_cluster_before_regression(const std::vector<measure *> & data, metric_type mtype, const double cl_probability, const double lb, const double ub, std::vector<measure *> & f_data) {
	f_data.clear();
	double prob = 0;
	for (measure *m: data) {
		uint64_t mvalue = m->measures[mtype];
		if (mvalue > lb && mvalue <= ub) {
			prob += 1;
			f_data.push_back(m);
		}
	}
	prob /= data.size();
	//std::cout << "D: " << prob << " - " << cl_probability << std::endl;
	if (abs(prob - cl_probability) > MAX_CLUST_DST)
		return false;
	
	return true;	
}

bool stats::validate_clusters(const std::vector<measure *> & data, metric_type mtype, const std::vector<struct cluster> & clusters) {
	uint32_t * counts = new uint32_t[clusters.size() + 1]{0};
	for (measure *m: data) {
		bool cfound = false;
		uint64_t mvalue = m->measures[mtype];
		//std::cout << "Checking " << mvalue << std::endl;
		for (int cc = 0; cc < clusters.size(); cc++) {
			if ( (mvalue > clusters[cc].lower_bound) && mvalue <= clusters[cc].upper_bound) {
				counts[cc] += 1;
				cfound = true;
				break;
			}
		}
		if (!cfound) {
			counts[clusters.size()] += 1; // The "not found" cluster is the last
		}
	}
	double tot_not_in_cluster = 1.0;
	for (int cc = 0; cc < clusters.size(); cc++)
		tot_not_in_cluster -= clusters[cc].probability;

	// Now I want to check that the new probabilities are "similar" to the old ones...
	double prob = (double)counts[clusters.size()] / (double)data.size();
	if (abs(prob - tot_not_in_cluster) > MAX_CLUST_DST) {
		printf("%f of samples are not in any cluster! (It was %f)\n", prob, tot_not_in_cluster);
		return false;
	}	
	for (int cc = 0; cc < clusters.size(); cc++) {
		prob = (double)counts[cc] / (double)data.size();
		if (abs(prob - clusters[cc].probability) > MAX_CLUST_DST) {
			printf("Validation failed for cl %d: %f!\n", cc, prob - clusters[cc].probability);
			return false;
		}
	}
	delete[] counts;
	return true;	
}

bool stats::validate_constant(const std::vector<measure *> & data, metric_type mtype, int cvalue) {
	// this is constant, handle differently!
	bool is_c; uint64_t c_f;
	is_constant(data, mtype, is_c, c_f);
	//std::cout << "IC: " << is_c << ", val=" << c_f << ", cval=" << cvalue << std::endl;
	double dst;
	if (c_f != 0)
		dst = (double)c_f / (double)cvalue;
	else {
		dst = 1 + cvalue;
	}
	if (dst > MAX_CONST_DST || dst < 1.0/MAX_CONST_DST)
		std::cout << "DBG: " << is_c << " - " << dst << std::endl; 
	return (is_c && (dst >= 1.0/MAX_CONST_DST && dst <= MAX_CONST_DST));
}

bool stats::validate_regression(const std::vector<measure *> & data, std::string feature_key, metric_type mtype, multiple_regression * r) {
	// TODO: revisit the cross validation procedure
	// https://stats.stackexchange.com/questions/129937/how-to-compute-r-squared-value-when-doing-cross-validation
	// https://stats.stackexchange.com/questions/186396/appropriate-way-to-calculate-cross-validated-r-square

#if 0
	int cnt = 0; 
	SEXP data_mat;
	SEXP coeff;
	PROTECT(data_mat = allocMatrix(INTSXP, 2, data.size()));
	PROTECT(coeff = allocVector(REALSXP, MAX_DEGREE + 1));
	int *c_mat = INTEGER(data_mat); 
	for (int i = 0; i < data.size(); i++) {
		c_mat[2 * i] = data[i]->measures[mtype];;
		c_mat[2 * i + 1] = data[i]->get_feature_value(feature_key);
	}


	double *c_vec = REAL(coeff);
	for (int i = 0; i < MAX_DEGREE + 1; i++) {
		c_vec[i] = 0; 
	}
	for (int i = 0; i < r->degree + 1; i++) {
		c_vec[i] = r->coefficients[i]; 
	}

	double det = 0;

	// Setup a call to the R function
	SEXP cm_call;
	
	PROTECT(cm_call = lang4(install("check_model"), coeff, data_mat, ScalarLogical(r->isnlogn)));
	// Execute the function
	int errorOccurred;
	SEXP ret = R_tryEval(cm_call, R_GlobalEnv, &errorOccurred);
	double * val;
	if (!errorOccurred)
	{
		val = REAL(ret);
		det = val[0];
	} else {
		printf("### R returned error: %d\n", errorOccurred);
	}
	UNPROTECT(3);
	
	std::cout << "PD: " << r->determination << ", ND: " << det << std::endl;
	

	return det >= min_det;
#else
	return false;
#endif
}

bool feature_with_var(const std::vector<measure *> &in_data, const std::string &f) {
	for (int i = 1; i < in_data.size(); i++)
		if (in_data[i]->get_feature_value(f) != in_data[i-1]->get_feature_value(f))
			return true;

	return false;
}

void stats::find_noncorr_features(const std::vector<measure *> &in_data, const std::unordered_set<std::string> &features, std::vector<std::string> &noncorr_features) {
	utils::log(VL_DEBUG, "Filtering features " + std::to_string(features.size()));
	// BUILD A STRING -> IDX MAP
	std::vector<std::string> fn;
	for (std::string feat: features) {
		// Never use vptr for the actual regression
		if ((feat.find("vptr.") != 0) && feature_with_var(in_data, feat)) {
			fn.push_back(feat);
		}
	}
	unsigned long fsize = fn.size();
	utils::log(VL_DEBUG, "Nonzero features " + std::to_string(fsize));
	SEXP mat;
	PROTECT(mat = allocMatrix(INTSXP, in_data.size(), fsize));
	int *c_mat = INTEGER(mat);

	for (int f = 0; f < fsize; f++) {
		for (int i = 0; i < in_data.size(); i++) {
			c_mat[in_data.size() * f + i] = in_data[i]->get_feature_value(fn[f]);
		}
	}
	// Setup a call to the R function
	SEXP gbm_call;
	PROTECT(gbm_call = lang2(install("cor_wrap"), mat));
	// Execute the function
	int errorOccurred;
	SEXP ret = R_tryEval(gbm_call, R_GlobalEnv, &errorOccurred);
	double * val = REAL(ret);
	
	std::unordered_set<std::string> to_be_dropped;
	for (int x = 0; x < fsize; x++) {
		for (int y = 0; y < x; y++) {
			if (val[x*fsize+y] >= CORRELATION_THRESHOLD || val[x*fsize+y] <= -CORRELATION_THRESHOLD) {
				// just pick one randomly; the correlation property is transitive
				to_be_dropped.insert(fn[x]);
			}
		}
	}

	for (std::string feat: fn)
		if (to_be_dropped.find(feat) == to_be_dropped.end())
			noncorr_features.push_back(feat);
	// Unprotect add1_call and arg
	UNPROTECT(2);
	utils::log(VL_DEBUG, "Dropped " + std::to_string(features.size() - noncorr_features.size()) + " intercorrelated features\n");
}

void * stats::prepare_data_for_r(const std::vector<measure *> &data, 
	const std::vector<std::string> &features,
	std::vector<std::string> &features_interaction,
	std::vector<std::string> &quad_features,
	std::vector<std::string> &quad_features_interaction,
	std::vector<std::string> &log_features,
	std::vector<std::string> &log_features_interaction,
	// linear
	std::unordered_map<std::string, std::string> &names_map_to_r,
	std::unordered_map<std::string, std::string> &names_map_to_c,
	// quad
	std::unordered_map<std::string, std::string> &quad_names_map_to_r,
	std::unordered_map<std::string, std::string> &quad_names_map_to_c,
	// log
	std::unordered_map<std::string, std::string> &log_names_map_to_r,
	std::unordered_map<std::string, std::string> &log_names_map_to_c,
	const metric_type mtype) 
{
	// Allocate an R matrix and copy the C array into it.
	int fsize = features.size();
	std::vector<std::string> new_features;
	SEXP mat;
	PROTECT(mat = allocMatrix(INTSXP, features.size()+1, data.size()));
	int *c_mat = INTEGER(mat);
	for (int i = 0; i < data.size(); i++) {
		c_mat[(1 + fsize) * i] = data[i]->measures[mtype];
	}
	int fidx = 0;

	// Main terms (linear, quadratic, nlogn)
	for (std::string f: features) {
		// Copy the actual data to the R buffer 
		for (int i = 0; i < data.size(); i++) {	
			c_mat[(1 + fsize) * i + fidx + 1] = data[i]->get_feature_value(f);
		}
		// linear terms
		std::string r_f 	= "mat[" + std::to_string(fidx + 2) + ",]";
		names_map_to_r[f] 	= r_f;
		names_map_to_c[r_f] 	= f;
		utils::log(VL_DEBUG, f + " goes to " + r_f);

		// quad terms
		std::string quad_f 		= f + "^2";
		std::string quad_r_f 		= "I(" + r_f + "^2)";
		quad_names_map_to_r[quad_f] 	= quad_r_f;
		quad_names_map_to_c[quad_r_f] 	= quad_f;
		quad_features.push_back(quad_f);

		// log terms
		std::string log_f 		= f + "^" + std::to_string(LOG_DEG);
		std::string log_r_f 		= "I(" + r_f + " * log2(" + r_f + "))";
		log_names_map_to_r[log_f] 	= log_r_f;
		log_names_map_to_c[log_r_f] 	= log_f;
		log_features.push_back(log_f);

		//std::cout << "mat[" << std::to_string(fidx + 2) << ",] ==> " << f << std::endl;
		fidx++;
 	}

	// Interaction terms
	for (std::string f: features) {
		for (std::string ff: features) {
			if (f.compare(ff) < 0) {
				// Linear terms
				features_interaction.push_back(f+":"+ff);
				names_map_to_r[f+":"+ff] = names_map_to_r[f] + ":" + names_map_to_r[ff]; 	
				names_map_to_c[names_map_to_c[f] + ":" + names_map_to_c[ff]] = f+":"+ff;
				
				// Quad terms 
				// x^2:y 	
				std::string quad_c_name = f+"^2:"+ff;
				quad_names_map_to_r[quad_c_name] = "I("+names_map_to_r[f] + "^2):" + names_map_to_r[ff]; 	
				quad_names_map_to_c[names_map_to_c[f] + "^2:" + names_map_to_c[ff]] = quad_c_name;
				quad_features_interaction.push_back(quad_c_name);

				// x:y^2 
				quad_c_name = f+":"+ff+"^2";
				quad_names_map_to_r[quad_c_name] = names_map_to_r[f] + ":I(" + names_map_to_r[ff]+"^2)"; 	
				quad_names_map_to_c[names_map_to_c[f] + ":" + names_map_to_c[ff]+"^2"] = quad_c_name;
				quad_features_interaction.push_back(quad_c_name);
				
				// x^2:y^2 	
				quad_c_name = f+"^2:"+ff+"^2";
				quad_names_map_to_r[quad_c_name] = "I("+names_map_to_r[f] + "^2):I(" + names_map_to_r[ff]+"^2)"; 	
				quad_names_map_to_c[names_map_to_c[f] + "^2:" + names_map_to_c[ff]+"^2"] = quad_c_name;
				quad_features_interaction.push_back(quad_c_name);
				
				// Log terms 
				// xlog(x):y 	
				std::string log_c_name = f+"^" + std::to_string(LOG_DEG) +":"+ff;
				log_names_map_to_r[log_c_name] = "I(" + names_map_to_r[f] + " * log2("+names_map_to_r[f] + ")):" + names_map_to_r[ff]; 	
				log_names_map_to_c[names_map_to_c[f] + "^" + std::to_string(LOG_DEG) +":" + names_map_to_c[ff]] = log_c_name;
				log_features_interaction.push_back(log_c_name);

				// x:ylog(y) 
				log_c_name = f+":"+ff+"^" + std::to_string(LOG_DEG);
				log_names_map_to_r[log_c_name] = names_map_to_r[f] + ":I(" + names_map_to_r[ff] + " * log2(" + names_map_to_r[ff]+"))"; 	
				log_names_map_to_c[names_map_to_c[f] + ":" + names_map_to_c[ff]+"^2"] = log_c_name;
				log_features_interaction.push_back(log_c_name);
				
				// xlog(x):ylog(y) 	
				log_c_name = f+"^" + std::to_string(LOG_DEG) +":"+ff+"^" + std::to_string(LOG_DEG);
				log_names_map_to_r[log_c_name] = "I(" + names_map_to_r[f] + " * log2("+ names_map_to_r[f] + ")):I(" + names_map_to_r[ff] + " * log2(" + names_map_to_r[ff]+"))"; 	
				log_names_map_to_c[names_map_to_c[f] + "^" + std::to_string(LOG_DEG) + ":" + names_map_to_c[ff]+"^" + std::to_string(LOG_DEG)] = log_c_name;
				log_features_interaction.push_back(log_c_name);
				
			}
		}
	}
	return mat;	
}

bool vector_contains(const std::vector<std::string> &v, const std::string &s) {
	for (std::string ss: v)
		if (ss == s)
			return true;
	return false;
}

std::string get_c_feat_name(
	const std::vector<std::string> &features_to_use, 
	const std::vector<std::string> &quad_features_to_use, 
	const std::vector<std::string> &log_features_to_use, 
	const std::vector<std::string> &features_to_use_interaction, 
	const std::vector<std::string> &quad_features_to_use_interaction,
	const std::vector<std::string> &log_features_to_use_interaction,
	const unsigned i,
	const unsigned degree 
) {
	size_t ftus = features_to_use.size();
	size_t qftus = quad_features_to_use.size();
	size_t lftus = log_features_to_use.size();
	size_t ftuis = features_to_use_interaction.size();
	size_t qftuis = quad_features_to_use_interaction.size();
	size_t lftuis = log_features_to_use_interaction.size();
	switch (degree) {
		case 1:
			if (i >= 0 && i < ftus)
				return features_to_use[i];
			else if (i >= ftus && i < ftus + ftuis)
				return features_to_use_interaction[i - ftus];
			break;

		case 2:
			if (i >= 0 && i < ftus)
				return features_to_use[i];
			else if (i >= ftus && i < ftus + qftus)
				return quad_features_to_use[i - ftus];
			else if (i >= ftus + qftus && i < ftus + qftus + ftuis)
				return features_to_use_interaction[i - ftus - qftus];
			else
				return quad_features_to_use_interaction[i - ftus - qftus - ftuis];
			break; 
		
		case 3:
			if (i >= 0 && i < ftus)
				return features_to_use[i];
			else if (i >= ftus && i < ftus + lftus)
				return log_features_to_use[i - ftus];
			else if (i >= ftus + lftus && i < ftus + lftus + ftuis)
				return features_to_use_interaction[i - ftus - lftus];
			else
				return log_features_to_use_interaction[i - ftus - lftus - ftuis];
			break; 
	}
	return "no_c_name_this_should_not_happen";
}

void stats::free_r_memory(const int howmany) {
	UNPROTECT(howmany);
}

bool stats::compute_multiple_regression(const std::vector<measure *> &data, 
	const std::vector<std::string> &features_to_use, 
	const std::vector<std::string> &quad_features_to_use, 
	const std::vector<std::string> &log_features_to_use, 
	const std::vector<std::string> &features_to_use_interaction, 
	const std::vector<std::string> &quad_features_to_use_interaction, 
	const std::vector<std::string> &log_features_to_use_interaction, 
	metric_type mtype, 
	std::vector<std::string> &used_features, 
	double &det, 
	double &intercept, 
	std::vector<std::vector<double>> &coefficients, 
	double &bic, 
	const int degree, 
	void * matp, 
	const std::unordered_map<std::string, std::string> &names_map_to_r, 
	const std::unordered_map<std::string, std::string> &names_map_to_c, 
	const std::unordered_map<std::string, std::string> &quad_names_map_to_r, 
	const std::unordered_map<std::string, std::string> &quad_names_map_to_c,
	const std::unordered_map<std::string, std::string> &log_names_map_to_r, 
	const std::unordered_map<std::string, std::string> &log_names_map_to_c,
	std::unordered_map<std::string, double> &coefficients_map,
	const bool refining,
	const double min_det
	)
{
	utils::log(VL_DEBUG, "Finding multiv regr for " + std::to_string(features_to_use.size()) + " features and " + std::to_string(data.size()) +" dp\n");
	SEXP mat = (SEXP)matp;
	
	if (!features_to_use.size()) {
		utils::log(VL_DEBUG, "No features");
		return false;
	}

	// Setup a call to the R function
	SEXP gbm_call;
	std::string cpp_frm = "mat[1,] ~ ";
	for (std::string f: features_to_use) {
		if (names_map_to_r.find(f) != names_map_to_r.end())
			cpp_frm += " + " + names_map_to_r.at(f);
		// Can happen from the second round
		else if (quad_names_map_to_r.find(f) != quad_names_map_to_r.end()) 
			cpp_frm += " + " + quad_names_map_to_r.at(f);
		else if (log_names_map_to_r.find(f) != log_names_map_to_r.end()) 
			cpp_frm += " + " + log_names_map_to_r.at(f);
		else {
			utils::log(VL_ERROR, "Cannot find R's for " + f);
			return false;
		}
	}
	
	// + 1 because there is also the intercept, which is always the first!
	int n = features_to_use.size() + 1;
	
	if (!refining && degree == 2) {
		for (std::string f: quad_features_to_use) {
			cpp_frm += " + " + quad_names_map_to_r.at(f);
		}
		n += quad_features_to_use.size();
	} else if (!refining && degree == LOG_DEG) {
		for (std::string f: log_features_to_use) {
			cpp_frm += " + " + log_names_map_to_r.at(f);
		}
		n += log_features_to_use.size();
	}
	
	if (!refining) {	
		for (std::string f: features_to_use_interaction) {
			cpp_frm += " + " + names_map_to_r.at(f);
		}
		n += features_to_use_interaction.size();
	}
	if (!refining && degree == 2) {
		for (std::string f: quad_features_to_use_interaction) {
			cpp_frm += " + " + quad_names_map_to_r.at(f);
		}
		n += quad_features_to_use_interaction.size();
	} else if (!refining && degree == LOG_DEG) {
		for (std::string f: log_features_to_use_interaction) {
			cpp_frm += " + " + log_names_map_to_r.at(f);
		}
		n += log_features_to_use_interaction.size();
	}
	if (n >= data.size()) {
		utils::log(VL_DEBUG, "More features than data points, quitting. (" + std::to_string(n) + ", " + std::to_string(data.size()) + ")");
		return false;
	}
		
	SEXP formula;
	PROTECT(formula = Rf_mkString(cpp_frm.c_str()));
	PROTECT(gbm_call = lang3(install("get_multimodel"), mat, formula));
	//Language e("gm", "frm");
	// Execute the function
	int errorOccurred;
	SEXP ret = R_tryEval(gbm_call, R_GlobalEnv, &errorOccurred);
	double * model = REAL(ret);
	if (model == nullptr) {
		// Something bad happened
		return false;
	}
	// n = fn.size() + 1
	// model[0]: BIC
	// model[1]: r^2
	// model[2 ... n]: coefficients
	// model[2 + n ... 2n]: std. errors
	// model[2 + 2n ... 3n]: t values
	// model[2 + 3n ... 4n]: p values
	bic = model[0];
	if (!std::isfinite(bic))
		// Something went wrong, cannot use this regression
		return false;

	double rsquared = model[1];
	if (bic == 0 || rsquared == 0)
		// Couldn't compute model, error in R (NA?)
		return false;

	utils::log(VL_DEBUG, "BIC: " + std::to_string(bic));
	utils::log(VL_DEBUG, "R2: " + std::to_string(rsquared));
	int not_used_count = 0;
	std::vector<std::string> interaction_terms_to_be_added;
	std::multimap<double, std::string> top_features;

	for (int f = 0; f < n; f++) {
		double coefficient = model[2 + f];
		bool notnan = std::isfinite(coefficient);
		if (!notnan)
			not_used_count++;
		if (f > 0) {
			std::vector<double> cf;
			cf.push_back(coefficient);
			coefficients.push_back(cf);
		}
		//double stderror = model[2 + f + n];
		//double tvalue = model[2 + f + 2*n];
		double pvalue = 0;
		if (notnan)
			pvalue = model[2 + f + n - not_used_count];

		if (f == 0) {
			//printf("Feat %d: %.5e | %.5e\t%s\n", f, coefficient, pvalue, "Intercept");
		} else {
			// No need to check for refining; 
			// everything is inside features_to_use, the first
			std::string c_feat_name = get_c_feat_name(features_to_use, quad_features_to_use, log_features_to_use, features_to_use_interaction, quad_features_to_use_interaction, log_features_to_use_interaction, f-1, degree);
		
			top_features.insert(make_pair(pvalue, c_feat_name));
			coefficients_map[c_feat_name] = coefficient;
			if (notnan && coefficient != 0 && pvalue < GOOD_PVALUE && f > 0) {
				//printf("Feat %d: %.5e | %.5e\t (%s)\n", f, coefficient, pvalue, c_feat_name.c_str());
				int colon_pos = c_feat_name.find(":"); 
				if (colon_pos != std::string::npos) {
					std::string f1 = c_feat_name.substr(0, colon_pos);
					std::string f2 = c_feat_name.substr(colon_pos+1, std::string::npos);
					if (!vector_contains(used_features, f1))
						used_features.push_back(f1);	
					if (!vector_contains(used_features, f2))
						used_features.push_back(f2);	
					if (!vector_contains(interaction_terms_to_be_added, c_feat_name))
						interaction_terms_to_be_added.push_back(c_feat_name);
				} else {
					if (!vector_contains(used_features, c_feat_name))
						used_features.push_back(c_feat_name);	
				} 
			}
		}
	}
	for (std::string ittba: interaction_terms_to_be_added)
		used_features.push_back(ittba);

	// If I'm taking too many features, it might be a good idea to choose only
	// the top_k (according to the p-value)
	if (rsquared > min_det && used_features.size() == 0) {
		//std::cout << "Good regression but unknown feature" << std::endl;
		int k = 0;
		// pretend we didn't fail and try again with the top_k features only
		for (std::pair<double, std::string> f: top_features) {
			if (k++ >= std::min(top_features.size() - 1, top_features.size() - DROP_K)) {
#if 0
				if (names_map_to_r.find(f.second) != names_map_to_r.end())
					std::cout << "Dropping " << names_map_to_r.at(f.second) << std::endl;
				else if (quad_names_map_to_r.find(f.second) != quad_names_map_to_r.end())
					std::cout << "Dropping " << quad_names_map_to_r.at(f.second) << std::endl;
				else if (log_names_map_to_r.find(f.second) != log_names_map_to_r.end())
					std::cout << "Dropping " << log_names_map_to_r.at(f.second) << std::endl;
				else {
					std::cout << "FIXME: Could not find cname for " << f.second << "; quitting" << std::endl;
					return false;
				}
#endif
				// If we're dropping a main feature and we still have the
				// interaction term, we're doing something useless
				// Workaround: drop another feature
				if (f.second.find(":") == std::string::npos) {
					for (std::string ittba: interaction_terms_to_be_added)
						if (utils::comp_contains(ittba, f.second)) {
							used_features.pop_back();
							break;
						}
					
				}
				continue;
			}
			std::string c_feat_name = f.second;
			int colon_pos = c_feat_name.find(":"); 
			if (colon_pos != std::string::npos) {
				std::string f1 = c_feat_name.substr(0, colon_pos);
				std::string f2 = c_feat_name.substr(colon_pos+1, std::string::npos);
				if (!vector_contains(used_features, f1))
					used_features.push_back(f1);	
				if (!vector_contains(used_features, f2))
					used_features.push_back(f2);	
				if (!vector_contains(interaction_terms_to_be_added, c_feat_name))
					interaction_terms_to_be_added.push_back(c_feat_name);
				} else {
					if (!vector_contains(used_features, c_feat_name))
						used_features.push_back(c_feat_name);	
				} 
		}
		for (std::string ittba: interaction_terms_to_be_added)
			used_features.push_back(ittba);
		det = 0;
		bic = DBL_MAX;
		return true;
	}

	det = rsquared;
	intercept = model[2];

	UNPROTECT(2);
	return rsquared > min_det && used_features.size() > 0;
}

void stats::kde_clustering(const std::vector<measure *> & data, const metric_type mtype, std::vector<struct cluster> & clusters, bool R_needs_init) {
	//std::cout << "Gonna cluster a vector of " << data.size() << std::endl;

	double tot, sqr_tot, min, max, tot_f, sqr_tot_f, var, f_var;
	compute_tot_min_max(data, "", mtype, tot, sqr_tot, min, max, tot_f, sqr_tot_f, 0);
	
	if (min == max) {
		std::cout << "max == min, this should not happen!" << std::endl;
		return; // no point in clustering
	}
	compute_variances(data, tot, sqr_tot, tot_f, sqr_tot_f, var, f_var, 0, 0, "", mtype);
	// Allocate an R vector and copy the C array into it.
	SEXP arg;
	PROTECT(arg = allocVector(INTSXP, data.size()));
	int * r_vec = INTEGER(arg);

	for (int i = 0; i < data.size(); i++) {
		uint64_t mvalue = data[i]->measures[mtype];
		if (mvalue > 2147483647u) {
			printf("The value might be too big for R, check!\n");
			exit(-1);
		}
		r_vec[i] = mvalue;
	}

	// Setup a call to the R function
	SEXP kde_clustering;
	//PROTECT(kde_clustering = lang2(install("kde_clustering"), arg));
	PROTECT(kde_clustering = lang2(install("vkde_clustering"), arg));
	 
	// Execute the function
	int errorOccurred;
	SEXP ret = R_tryEval(kde_clustering, R_GlobalEnv, &errorOccurred);

	if (!errorOccurred)
	{
		double *kde = REAL(ret);

		printf("R returned: ");
		for (int i = 0; i < LENGTH(ret); i++)
			printf("%0.1f, ", kde[i]);
		printf("\n");
		create_clusters(data, mtype, kde, min, max, clusters);

		//variable_kde_clustering(data, mtype, *bandwidth, min, max, sqrt(var), clusters);
		// FIXME vkde failed, probably because of "Too many..."
		//if (clusters.size() == 0)
			
	} else {
		printf("R returned error: %d\n", errorOccurred);
		printf("MIN - MAX: %f - %f\n", min, max);
	}
	
	// Unprotect add1_call and arg
	UNPROTECT(2);

}


void stats::create_clusters(const std::vector<measure *> & data, const metric_type mtype, const double * kde, const double min, const double max, std::vector<struct cluster> & clusters) {
	std::vector<double> centroids, boundaries;

	int m = 0;
	int nmax = kde[m++];
	for (; m < nmax + 1; m++)
		centroids.push_back(kde[m]);
	int nmin = kde[m++];
	for (; m < nmin + 2 + nmax; m++)
		boundaries.push_back(kde[m]);
	
	if (max == min) {
		std::cout << "something wrong with kde: " << min << " - " << max << std::endl;
		return;
	}

	/*
	std::cout << "Centroids: ";
	for (int i = 0; i < centroids.size(); i++)
		std::cout << " " << centroids[i];
	std::cout << std::endl;

	std::cout << "BDries: ";
	for (int i = 0; i < boundaries.size(); i++)
		std::cout << " " << boundaries[i];
	std::cout << std::endl;
	*/

	int cpos = 0, bpos = 0, win = 0;
	bool fixing_occurred = false;
	struct cluster c;

	if (centroids.size() && boundaries.size()) {
		bool is_centroid, was_centroid;
		int itr = 0;
		while (cpos < centroids.size() && bpos < boundaries.size()) {
			bool round_fix_occurred = false;
			was_centroid = is_centroid;
			//std::cout << "C: " << centroids[cpos] << " | B: " << boundaries[bpos] << std::endl;
			is_centroid = centroids[cpos] < boundaries[bpos];
			win = 0;
			if (itr > 0) {
				if (is_centroid == was_centroid) {
					// there IS something to fix
					// - compute the width of the window
					win = 1;
					if (is_centroid) {
						// need to increase the centroids index
						while ((cpos + win < centroids.size()) && (centroids[cpos + win] < boundaries[bpos])) {
							win++;
						}
					} else {
						// need to increase the boundaries index
						while ((bpos + win < boundaries.size()) && (centroids[cpos] > boundaries[bpos + win])) {
							win++;
						}
					}
					// - I have the window, tamper with the vectors
					if (is_centroid) {
						//std::cout << "CDeleting a window of " << win << ", from " << cpos << " to " << cpos + win << std::endl;
						if (cpos + win > centroids.size()) {
							std::cout << "Trying to erase past the end() of vector. " << cpos << " - " << win << " - " << centroids.size() << std::endl;
							exit(-1);
						}
						centroids.erase(centroids.begin() + cpos, centroids.begin() + cpos + win);
						round_fix_occurred = true;	
						fixing_occurred = true;
						//std::cout << "Centroids: ";
						//for (int i = 0; i < centroids.size(); i++)
						//	std::cout << " " << centroids[i];
						//std::cout << std::endl;
					} else {
						//std::cout << "BDeleting a window of " << win << ", from " << bpos << " to " << bpos + win << std::endl;
						boundaries.erase(boundaries.begin() + bpos, boundaries.begin() + bpos + win);
						fixing_occurred = true;
						round_fix_occurred = true;	
					}
				}
			}
			itr++;
			if (!round_fix_occurred) {
				if (centroids[cpos] < boundaries[bpos]) 
					cpos++;
				else
					bpos++;
			}
		}	
	}
	//std::cout << "F2: " << win << ", " << bpos << ", " << cpos << std::endl;
	if (bpos < boundaries.size() - 1) {
		if (boundaries[bpos] < centroids[cpos - 1])
			bpos++;
		// keep the last
		//std::cout << "Deleting bdrs " << bpos << " to " << boundaries.end() - boundaries.begin() << std::endl;
		boundaries.erase(boundaries.begin() + bpos, boundaries.end());
		fixing_occurred = true;
	}
	if (cpos < centroids.size() - 1) {
		if (boundaries[bpos] > centroids[cpos])
			cpos++;
		// keep the first 
		//std::cout << "Deleting Ctrds " << cpos << " to " << centroids.end() - centroids.begin() << std::endl;
		centroids.erase(centroids.begin() + cpos, centroids.end());
		fixing_occurred = true;
	}

	if (fixing_occurred) {
		std::cout << "FIXME: CHECK THIS!" << std::endl;
		std::cout << "Centroids: ";
		for (int i = 0; i < centroids.size(); i++)
			std::cout << " " << centroids[i];
		std::cout << std::endl;

		std::cout << "BDries: ";
		for (int i = 0; i < boundaries.size(); i++)
			std::cout << " " << boundaries[i];
		std::cout << std::endl;
	}

	cpos = 0;
	bpos = 0;
	while (cpos < centroids.size() && bpos < boundaries.size()) {
		if (centroids[cpos] < boundaries[bpos]) {
			// I reached a centroid, must add a new cluster
			c.centroid = centroids[cpos];
			if (bpos > 0)
				c.lower_bound = boundaries[bpos - 1];
			else 
				// Assuming a performance metric has a minimum value = 0
				c.lower_bound = 0; //-std::numeric_limits<double>::infinity();
			
			// Make sure the lower boundary is included in at least 1 cluster,
			// which must be the first
			if (clusters.size() == 0)
				c.lower_bound -= 1;
			
			if (bpos < boundaries.size())
				c.upper_bound = boundaries[bpos];
			else
				c.upper_bound = std::numeric_limits<double>::infinity();
			
			cpos++;
			clusters.push_back(c);
		} else {
			bpos++;
		}
	}
	while (cpos < centroids.size()) {
		// There should be at most one more...
		// Lower bound is the last upper bound, upper bound is infinity
		c.centroid = centroids[cpos];
		if (boundaries.size() > 0) 
			c.lower_bound = boundaries[boundaries.size() - 1];
		else
			c.lower_bound = 0;

		// Make sure the lower boundary is included in at least 1 cluster,
		// which must be the first
		if (clusters.size() == 0)
			c.lower_bound -= 1;
		c.upper_bound = std::numeric_limits<double>::infinity();
		clusters.push_back(c);
		cpos++; // Needed only to exit the loop
	}
	if (boundaries.size() > 0 && bpos < boundaries.size() - 1) {
		std::cout << "This should not happen! " << bpos << " - " << boundaries.size() << std::endl;
		exit(-1);
	}

	for (measure *m: data) {
		uint64_t mvalue = m->measures[mtype];
		for (int cc = 0; cc < clusters.size(); cc++) {
			// If it's the first cluster, we accept also values equal to the lower boundary (would be excluded otherwise)
			if ( mvalue > clusters[cc].lower_bound && mvalue <= clusters[cc].upper_bound) {
				clusters[cc].probability += 1;
				clusters[cc].variance += pow(mvalue - clusters[cc].centroid, 2); 
				break;
			}
		}
	}


	for (int cc = 0; cc < clusters.size(); cc++) {
		clusters[cc].variance /= clusters[cc].probability;
		clusters[cc].probability /= data.size();
	}
}
