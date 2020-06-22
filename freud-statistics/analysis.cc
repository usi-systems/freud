#include "analysis.hh"
#include <fstream>
#include <list>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <limits>

#include "plotter.hh"
#include "utils.hh"

#define MIN_PART_POINTS 5

std::vector<std::string> filter_strings = { "<=" , ">", "><", "==", "!=" };

// mim stands for measures_ids_map
analysis::analysis(const std::string &mname, method * m, metric_type mtype, std::unordered_map<uint64_t, measure *> * mim) {
	metric = mtype;
	mtd = m;
	mtd->name = mname;
	m_ids_map = mim;
	
	// Defined in stats.hh, can be overridden
	min_det = MIN_DET;

	//m->add_loops_as_feature();
	for (measure * mmm: mtd->data) {
		//std::cout << std::endl << std::endl << "T: " << mmm->measures[MT_TIME] << std::endl;
		std::cout << mmm->measures[MT_TIME] << " ";
		uint64_t csum = 0;
		for (uint64_t c_id_64: mmm->children_ids) {
				if (mim->find(c_id_64) != mim->end()) {
					measure * mm = mim->at(c_id_64);
					std::cout << mm->measures[MT_TIME] << " ";
				} else {
					std::cout << "Couldn't find " << c_id_64 << std::endl;
				}
		}
		std::cout << std::endl;
	}
}

bool analysis::cluster() {
	return cluster(mtd->performance_annotation.clusters);
}

bool analysis::cluster(std::vector<struct cluster> &result) {
	result.clear();
	if (mtd->data.size() < 3) {
		utils::log(VL_DEBUG, "Too few samples, not gonna cluster");
		return false;
	}
	stats::kde_clustering(mtd->data, metric, result, false);
	
	// Ok, let's build the actual cluster here
	if (result.size() == 0) {
		// constant data, maybe?
		utils::log(VL_ERROR, "Something wrong with clusters!");
		exit(-1);
	}
	
	return true;
}


// returns true iff c1 is "contained" in c2
bool regr_is_contained(struct corr_with_constraints c1, struct corr_with_constraints c2) {
	if (c1.metric != c2.metric 
		//|| c1.path_conditions_vec.size() == c2.path_conditions_vec.size()
	)
		return false;

	return false;

	// TODO: is this still needed?

	/*
	univariate_regression * r1 = c1.reg;
	univariate_regression * r2 = c2.reg;
	
	if (r1 && !r2
		|| !r1 && r2)
		return false;

	if (!r1 && !r2) {
		// TODO: implement this
		return false;
	}
	

	if (r1->get_fkeyidx() != r2->get_fkeyidx())
		return false;

	// At this point I know that c1 and c2:
	// - have the same metric, and var|feature	
	
	// CASE 1: 
	// c1 and c2 have similar univariate_regression, but c1 has more constraints than c2
	if (r1->degree == r2->degree 
		&& r1->isnlogn == r2->isnlogn
		&& c1.path_conditions_vec.size() > c2.path_conditions_vec.size()
		// && similar determination?
	) {
		return true;
	}

	return false;
	*/
}


/* ************************

   REGRESSION TREE ANALYIS

*  ***********************/ 
bool analysis::find_regressions(double min_r2) {
	min_det = min_r2;
	return find_regressions();
}

// This is the entry point in the mixture model analysis
bool analysis::find_regressions() {
	bool res = compute_best_multiple_regression(mtd->performance_annotation.regressions);

	std::vector<struct corr_with_constraints> filtered_results;
	bool filtered = false;

	// Filter regressions (quadratic complexity?)
	if (res && mtd->performance_annotation.regressions.size() > 1) {
		for (int i = 0; i < mtd->performance_annotation.regressions.size(); i++) {
			bool keep = true;
			for (int j = 0; j < mtd->performance_annotation.regressions.size(); j++) {
				if (j == i)
					continue;

				if (regr_is_contained(mtd->performance_annotation.regressions[i], mtd->performance_annotation.regressions[j])) {
					keep = false;
					break;
				}		
			}
			if (keep) {
				filtered_results.push_back(mtd->performance_annotation.regressions[i]);
			} else {
				filtered = true;
			}
		}
	}

	if (filtered) {
		mtd->performance_annotation.regressions	= filtered_results;
	}		

	return res;
}

bool analysis::compute_best_multiple_regression(std::vector<struct corr_with_constraints> &result) {
	// Preliminary check: do we have any features?
	if (mtd->feature_set.size() == 0) {
		utils::log(VL_DEBUG, "Skipping " + mtd->name +  " with no feature");
		// I should still check whether it is constant
		bool is_c; uint64_t c_f;
		stats::is_constant(mtd->data, metric, is_c, c_f);
		if (is_c) {
			struct corr_with_constraints cwc;
			cwc.constant = true;
			cwc.constant_value = c_f;
			cwc.metric = metric;
			cwc.data_points = mtd->data;
			cwc.reg = 0;
			result.push_back(cwc);
			return true;
		}

		return false;
	}

	std::vector<struct branch_corr> branches = mtd->find_correlated_branches();
	std::vector<struct branch_corr> enum_branches = mtd->get_enum_partitions();

	// add the enum "branches"
	utils::log(VL_DEBUG, std::to_string(enum_branches.size()) + " enums for " + mtd->name);
	for (struct branch_corr bc: enum_branches) {
		utils::log(VL_DEBUG, bc.feature);
		for (int p: bc.pivots)
			utils::log(VL_DEBUG, std::to_string(p));
	}

	// If partitioning is needed, try enums first, since they're bases on the code, thus more promising
	branches.insert(branches.begin(), enum_branches.begin(), enum_branches.end());

	utils::log(VL_DEBUG, "############"); 
	utils::log(VL_DEBUG, "Analyzing " + mtd->name +  " with " + std::to_string(mtd->feature_set.size()) + " and " + std::to_string(branches.size()) + " branches");
	for (struct branch_corr bc: branches) {
		utils::log(VL_DEBUG, bc.feature + "; first pivot: " + std::to_string(bc.pivots[0]) + " (" + bc.feature + ")");
	}
	std::vector<struct path_filter> empty_pconds;
	explore_regression_tree(mtd->data, branches, result, 0, empty_pconds, mtd->feature_set, ORIGINAL);
	utils::log(VL_DEBUG, "############"); 

#if 1
	if (result.size() == 0) {
		// Try RemoveNoise
		// TRY TO FIND THE LOWER BOUNDARY FOR PERFORMANCE
		if (metric == MT_TIME || metric == MT_LOCK || metric == MT_WAIT) {
			for (std::string f: mtd->feature_set) {
				// Try to remove some points from the input data
				// I know the variance of the feature is non-zero
				std::vector<measure *> rn_data;
				utils::remove_addnoise(f, metric, mtd->data, rn_data);
				utils::log(VL_DEBUG, "RN (" + f + ": in = " + std::to_string(mtd->data.size()) + ", out = " + std::to_string(rn_data.size()));
				explore_regression_tree(rn_data, branches, result, 0, empty_pconds, mtd->feature_set, REMOVED_NOISE);
			}
		}
	}
	if (result.size() == 0) {
		// Try MainTrend 
		// TRY TO FIND THE MAIN TREND FOR PERFORMANCE
		if (metric == MT_TIME || metric == MT_LOCK || metric == MT_WAIT) {
			for (std::string f: mtd->feature_set) {
				// Try to remove some points from the input data
				// I know the variance of the feature is non-zero
				std::vector<measure *> mt_data;
				utils::get_main_trend(f, metric, mtd->data, mt_data);
				utils::log(VL_DEBUG, "MT (" + f + ": in = " + std::to_string(mtd->data.size()) + ", out = " + std::to_string(mt_data.size()));
				explore_regression_tree(mt_data, branches, result, 0, empty_pconds, mtd->feature_set, MAIN_TREND);
			}
		}
	}
#endif


	// ******* CLUSTER ANALYSIS *******

	if (result.size() == 0) {
		// FIXME: this "is_constant" check should be unnecessary, but apparently it is not...
		bool is_c; uint64_t c_f;
		stats::is_constant(mtd->data, metric, is_c, c_f);
		if (is_c) {
			struct corr_with_constraints cwc;
			cwc.constant = true;
			cwc.constant_value = c_f;
			cwc.metric = metric;
			cwc.data_points = mtd->data;
			cwc.reg = 0;
			result.push_back(cwc);
			return true;
		}
		
		int cl_found = 0;
		std::vector<std::vector<struct corr_with_constraints>> cl_results;
		utils::log(VL_DEBUG, "### REGR after CLUSTER attempt ###");
		// Next attempt, cluster and then look for regr within the clusters
		std::vector<struct cluster> cl;
		cluster(cl);
		std::vector<measure *> cl_data;
		cl_results.resize(cl.size());
		for (int cc = 0; cc < cl.size(); cc++) {
			//FIXME: probability here might be uninitialized, according to valgrind
			utils::log(VL_DEBUG, "-- CL " + std::to_string(cc) + ", pr: " + std::to_string(cl[cc].probability) + " --, var: " + std::to_string(cl[cc].variance));
			utils::log(VL_DEBUG, "lb " + std::to_string(cl[cc].lower_bound) + " -- ub " + std::to_string(cl[cc].upper_bound));

			// PREPARE THE NEW DATA SET
			cl_data.clear();
			for (measure *m: mtd->data) {
				uint64_t mvalue = m->measures[metric];
				if (mvalue > cl[cc].lower_bound && mvalue <= cl[cc].upper_bound) 
					cl_data.push_back(m);
			}
		
			// COMPUTE THE REGR ON THE NEW DATA SET
			explore_regression_tree(cl_data, branches, cl_results[cc], 0, empty_pconds, mtd->feature_set, ORIGINAL);
			cl_found += cl_results[cc].size();
			utils::log(VL_DEBUG, "Found new :" + std::to_string(cl_results[cc].size()));
		}

		// IF CONSECUTIVE CLUSTERS HAVE "VERY SIMILAR" REGRESSIONS, MERGE THEM
		int size = cl.size();
		for (int cc = 0; cc < size - 1; cc++) {
			if (utils::similar_regressions(cl_results[cc], cl_results[cc + 1])) {
				utils::log(VL_DEBUG, "Merging clusters " + std::to_string(cc) + " and " + std::to_string(cc + 1));
				cl[cc].upper_bound = cl[cc + 1].upper_bound;
				cl[cc].probability += cl[cc + 1].probability;
				// FIXME: there are better ways to determine the new centroid
				cl[cc].centroid = (cl[cc].centroid + cl[cc + 1].centroid) / 2;
				// Compute the new univariate_regression!
				cl_data.clear();
				for (measure *m: mtd->data) {
					uint64_t mvalue = m->measures[metric];
					if (mvalue > cl[cc].lower_bound && mvalue <= cl[cc].upper_bound) 
						cl_data.push_back(m);
				}
				cl_results[cc].clear();
				explore_regression_tree(cl_data, branches, cl_results[cc], 0, empty_pconds, mtd->feature_set, ORIGINAL);
				for (int ccc = cc + 1; ccc < size - 1; ccc++) {
					cl_results[ccc] = cl_results[ccc + 1];
					cl[ccc] = cl[ccc + 1];
				}
				size--;
				cc--;
			}
		}
		cl.resize(size);
		cl_results.resize(size);
		
		// IF I FOUND SOMETHING, TRY TO CATCH CLUSTER-DEFINING FEATURES
		if (cl_found > 0) {
			bool not_overlapping_found = false;
			utils::log(VL_DEBUG, "Found " + std::to_string(cl_found));
			for (std::string fkey: mtd->feature_set) {
				bool not_overlapping = true;
				std::vector<int> min, max;
				min.resize(cl.size()); 
				max.resize(cl.size()); 
				for (int cc = 0; cc < cl.size(); cc++) {
					min[cc] = std::numeric_limits<int>::max();
					max[cc] = std::numeric_limits<int>::min();
				}

				for (measure *m: mtd->data) {
					uint64_t mvalue = m->measures[metric];
					for (int cc = 0; cc < cl.size(); cc++) {
						if (mvalue > cl[cc].lower_bound && mvalue <= cl[cc].upper_bound) {
							int fvalue = m->get_feature_value(fkey);
							if (fvalue > max[cc])
								max[cc] = fvalue;
							if (fvalue < min[cc])
								min[cc] = fvalue;
							break;
						}
					}
				}
				// Check if there is a partitioning
				// n^2 check for overlaps, can do better
				for (int cc = 0; cc < cl.size() && not_overlapping; cc++) {
					for (int cc2 = cc + 1; cc2 < cl.size() && not_overlapping; cc2++) {
						int bigger_cl, smaller_cl; 
						if (max[cc2] > min[cc]) {
							bigger_cl = cc2;
							smaller_cl = cc;
						} else if (max[cc] > min[cc2]) {
							bigger_cl = cc;
							smaller_cl = cc2;
						} else {
							not_overlapping = false;
							break;
						}

						bool clean_split = max[smaller_cl] < min[bigger_cl];
						if (!clean_split &&
								// The following line checks whether there is a decent split.
								// Which means if the overlap between the two clusters is smaller than 5% of the smallest cluster
								!(max[smaller_cl] - min[bigger_cl] < (0.25 * std::min(max[cc] - min[cc], max[cc2] - min[cc2])))
								) {
							not_overlapping = false;
							//std::cout << "Not_ov: c1=" << cc << ", c2=" << cc2 << ", M1=" << max[cc] << ", m1=" << min[cc] << ", M2=" << max[cc2] << ", m2=" << min[cc2] << std::endl;
							//std::cout << "Intersection: " << max[smaller_cl] - min[bigger_cl] << std::endl;
							//std::cout << "Max allowed: " << 0.25 * std::min(max[cc] - min[cc], max[cc2] - min[cc2]) << std::endl;
						}
					}
				}
				utils::log(VL_DEBUG, fkey + ", not_overlapping: " + std::to_string(not_overlapping));
				for (int cc = 0; cc < cl.size(); cc++) {
					utils::log(VL_DEBUG, "Dbg " + std::to_string(cc) + "; min: " + std::to_string(min[cc]) + " - " + std::to_string(max[cc])); 
				}

				if (not_overlapping) {
					// We have valid conditions, add the info to the regr!
					for (int cc = 0; cc < cl.size(); cc++) {
						struct path_filter pf;
						pf.feature = fkey;
						pf.flt = FLT_BT;
						pf.lower_bound = min[cc];
						pf.upper_bound = max[cc];
						for (struct corr_with_constraints cwc: cl_results[cc]) {
							cwc.path_conditions_vec.push_back(pf);
							result.push_back(cwc);
						}	
					}
					not_overlapping_found = true;
					break; // stop at the first valid condition found
				} 
			}
			if (not_overlapping_found == false) {
				// we could not find any info, but we found the regressions anyway
				for (int cc = 0; cc < cl.size(); cc++) { 
					struct path_filter pf;
					pf.feature = "CL";
					pf.flt = FLT_EQ;
					pf.probability = cl[cc].probability;
					pf.cl_lower_bound = cl[cc].lower_bound;
					pf.cl_upper_bound = cl[cc].upper_bound;
					for (struct corr_with_constraints cwc: cl_results[cc]) { 
						cwc.path_conditions_vec.push_back(pf);
						result.push_back(cwc);
					}
				}
			}
		}
	}

	return result.size();
}

multiple_regression * analysis::compute_best_multiple_regression(const std::vector<measure *> &in_data, const std::unordered_set<std::string> &features) {
	// Throw everything at R;

	/* ############################# 
	*
	* Use cor() to drop correlated variables.
	* TODO: create equivalence classes
	*
	* ############################### */ 
	std::vector<std::string> noncorr_features, quad_features, features_interaction, quad_features_interaction, log_features, log_features_interaction, tmp_f_vector;
	
	stats::find_noncorr_features(in_data, features, noncorr_features);

	/* ############################# 
	*
	* Prepare the memory buffer with the data in R.
	* Create the [C]map fkey -> [R]col_name 
	*
	* ############################### */ 
	std::unordered_map<std::string, std::string> names_map_to_r, names_map_to_c;
	std::unordered_map<std::string, std::string> quad_names_map_to_r, quad_names_map_to_c;
	std::unordered_map<std::string, std::string> log_names_map_to_r, log_names_map_to_c;
	void * mat = stats::prepare_data_for_r(in_data, noncorr_features, features_interaction, quad_features, quad_features_interaction, log_features, log_features_interaction, names_map_to_r, names_map_to_c, quad_names_map_to_r, quad_names_map_to_c, log_names_map_to_r, log_names_map_to_c, metric); 

	
	/* ############################# 
	*
	* lm() to check whether a meaningful model exists with non correlated variables
	*
	* ############################### */
	 
	std::unordered_map<std::string, double> coefficients_map, best_coefficients_map;
	std::vector<std::string> used_features, best_used_features;
	double r2, intp, bic, best_bic = DBL_MAX, best_r2, best_intp;
	int best_degree;
	std::vector<std::vector<double>> cff, best_cff;
	bool found = false;
	// 1 n, 2 n^2, 3 nlogn
	for (int d = 1; d <= 3; d++) {
		// Apply an additional cost to higher order regressions
		double bic_penalty = BIC_DIFF * (d > 1);
		utils::log(VL_DEBUG, "-- REGRESSION DEG " + std::to_string(d) + " -- +" + std::to_string(bic_penalty));
		int prev_num_feat = names_map_to_r.size();
		if (d == 2)
			prev_num_feat += quad_names_map_to_r.size();
		else if (d == LOG_DEG)
			prev_num_feat += log_names_map_to_r.size();
		coefficients_map.clear();
		used_features.clear();
		cff.clear();
		// First run, throw every non-correlated feature at R
		tmp_f_vector = noncorr_features;
		found = stats::compute_multiple_regression(in_data, tmp_f_vector, quad_features, log_features, features_interaction, quad_features_interaction, log_features_interaction, metric, used_features, r2, intp, cff, bic, d, mat, names_map_to_r, names_map_to_c, quad_names_map_to_r, quad_names_map_to_c, log_names_map_to_r, log_names_map_to_c, coefficients_map, false, min_det);
		if (found && bic + bic_penalty < best_bic && used_features.size() == prev_num_feat) {
			best_bic = bic;
			best_r2 = r2;
			best_intp = intp;
			best_degree = d;
			best_cff = cff;
			best_used_features = used_features;
			best_coefficients_map = coefficients_map;
		}

		// - lm() to recompute the model if any feature was not significant
		utils::log(VL_DEBUG, "Used " + std::to_string(used_features.size()) + " / " + std::to_string(prev_num_feat));
		if (used_features.size() > prev_num_feat) {
			utils::log(VL_ERROR, "Used more features than available. This is a problem!");
			for (std::string s: used_features)
				std::cout << s << std::endl;
			exit(-1);
		}
		// keep iterating while we drop some features
		while (found && used_features.size() < prev_num_feat) {
			tmp_f_vector.clear();
			for (std::string f: used_features) {
				tmp_f_vector.push_back(f);
			}
			prev_num_feat = used_features.size();
			coefficients_map.clear();
			used_features.clear();
			cff.clear();
			found = stats::compute_multiple_regression(in_data, tmp_f_vector, quad_features, log_features, features_interaction, quad_features_interaction, log_features_interaction, metric, used_features, r2, intp, cff, bic, d, mat, names_map_to_r, names_map_to_c, quad_names_map_to_r, quad_names_map_to_c, log_names_map_to_r, log_names_map_to_c, coefficients_map, true, min_det);
			utils::log(VL_DEBUG, "Used " + std::to_string(used_features.size()) + " / " + std::to_string(prev_num_feat));
			if (used_features.size() > prev_num_feat) {
				utils::log(VL_ERROR, "Used more features than available. This is a problem!");
				exit(-1);
			}
			if (found && bic + bic_penalty < best_bic && used_features.size() == prev_num_feat) {
				best_bic = bic;
				best_r2 = r2;
				best_intp = intp;
				best_degree = d;
				best_cff = cff;
				best_used_features = used_features;
				best_coefficients_map = coefficients_map;
			}
		}
	}
	// Free the matrix for the data points in the R memory
	stats::free_r_memory(1);

	if (best_bic < DBL_MAX) {
		// Now I should have the best multiple linear multiple_regression model
		// Put it in multiple_regression
		std::vector<std::string> vnames;
		std::vector<int> degrees;
		std::vector<std::string> features_vec;
		std::unordered_map<std::string, std::string> vn_map;
		for (std::string f: best_used_features) {
			int colon_pos = f.find(":");
			std::string f1;
			std::string f2;
			f1 = f;
			if (colon_pos != std::string::npos) {
				f1 = f.substr(0, colon_pos);
				f2 = f.substr(colon_pos+1, std::string::npos);
			}
			vnames.push_back(f);
			degrees.push_back(best_degree);
			features_vec.push_back(f);
			std::string vname = utils::base_feature(f1);
			int dg = utils::get_degree(f1);
			if (dg == 2)
				vname += "^2";
			else if (dg == 3)
				vname += "*log(" + vname + ")";
			
			if (f2.size()) {
				std::string vname2 = utils::base_feature(f2);
				int dg = utils::get_degree(f2);
				if (dg == 2)
					vname2 += "^2";
				else if (dg == 3)
					vname2 += "*log(" + vname2 + ")";
				vname += "*" + vname2;
			}

			vn_map[f] = vname;
		}
		return new multiple_regression(best_r2, best_intp, features_vec, vnames, degrees, best_cff, best_coefficients_map, vn_map);
	}
	return nullptr;
}


bool analysis::explore_regression_tree(const std::vector<measure *> & data, const std::vector<struct branch_corr> branches, std::vector<corr_with_constraints> &result, int depth, const std::vector<struct path_filter> & path_conditions, const std::unordered_set<std::string> features, const enum phase ph) {
	if (data.size() <= MIN_PART_POINTS) // we use up to degree 3, so a regr is meaningful only with at least 5 points...
		return false;
	
	double max_det = 0;
	std::string best_feature;
	int best_pivot;
	bool base_is_constant = false;
	multiple_regression * base_multiple_regression = nullptr;

	if (features.size()) {
		int bcase_cvalue;
	
		/*
		 *	FIRST TRY TO FIND REGRESSIONS WITHOUT FURTHER PARTITIONING
		 */
		//printf("DATA HAS %lu, AT DEPTH %d\n", data.size(), depth);
		base_multiple_regression = compute_best_multiple_regression(data, features);

		if (base_multiple_regression) {
			//base_multiple_regression->print(std::cout);
			std::cout << std::endl;
	
			corr_with_constraints cwc;
			cwc.reg = base_multiple_regression;
			cwc.constant = false;
			cwc.data_points = data;
			cwc.addnoise_removed = (ph == REMOVED_NOISE);
			cwc.main_trend = (ph == MAIN_TREND);
			cwc.path_conditions_vec = path_conditions;
			cwc.metric = metric;
			result.push_back(cwc);
		}
		
		// I found something already...
		// I'm happy, and I won't dive in the tree
		if (result.size() > 0)
			return true;

		/*
		 *	NO LUCK, WE MUST TRY PARTITIONING
		 */
		bool could_split;
		std::vector<std::vector<measure *>> filtered_data;
		struct branch_corr br;
		do{
			if (depth >= branches.size()) {
				utils::log(VL_DEBUG, "Could not partition, dead end");
				return false; // out of branches, game over
			}

			br = branches[depth];
			filtered_data.clear();
			could_split = split_on_pivots(br.pivots, br.feature, data, filtered_data);
			if (could_split == false) {
				utils::log(VL_DEBUG, "Could not build partitions with feature " + br.feature);
				depth++;
			}
		} while (!could_split);
		
		if (!could_split) {
			// THIS SHOULD NEVER HAPPEN
			// could not partition anymore, dead end
			return false;
		}

		utils::log(VL_DEBUG, "branch " + std::to_string(br.baddr) + "; feature = " + br.feature + "; trying at depth " + std::to_string(depth));
		for (int i = 0; i < filtered_data.size(); i++) {
			utils::log(VL_DEBUG, "|||||||||| NEW NODE (" + std::to_string(depth) + ", " + std::to_string(i) + ")  |||||||||| ");
			if (i == 0) {
				utils::log(VL_DEBUG, "Processing part. " + std::to_string(i) + "; " + br.feature + " LE " + std::to_string(br.pivots[i]));
				std::vector<struct path_filter> pconds(path_conditions);
				pconds.emplace_back(br.feature, FLT_LE, 0, br.pivots[i]);
				
				// Not necessary, would be caught anyway
				// But avoids one useless recursive call with a copy
				// of the data
				if (filtered_data[i].size() < MIN_PART_POINTS) {
					utils::log(VL_DEBUG, "Not enough data points: " + std::to_string(filtered_data[i].size()));
					continue;
				}
			
				explore_regression_tree(filtered_data[i], branches, result, depth+1, pconds, features, ph);
				std::cout << "Returned " << i << "; " << br.feature << " LE " << br.pivots[i] << std::endl;
			}
			if (i == filtered_data.size() - 1) {
				utils::log(VL_DEBUG, "Processing part. " + std::to_string(i) + "; " + br.feature + " GT " + std::to_string(br.pivots[i - 1]));
				std::vector<struct path_filter> pconds(path_conditions);
				pconds.emplace_back(br.feature, FLT_GT, br.pivots[i - 1], 0);
				
				if (filtered_data[i].size() < MIN_PART_POINTS) {
					utils::log(VL_DEBUG, "Not enough data points: " + std::to_string(filtered_data[i].size()));
					continue;
				}
				
				explore_regression_tree(filtered_data[i], branches, result, depth+1, pconds, features, ph);
			}
			if (i > 0 && i < filtered_data.size() - 1) {
				utils::log(VL_DEBUG, "Processing part. " + std::to_string(i) + "; " + br.feature + " BT " + std::to_string(br.pivots[i - 1]) + " - " + std::to_string(br.pivots[i]));
				std::vector<struct path_filter> pconds(path_conditions);
				pconds.emplace_back(br.feature, FLT_BT, br.pivots[i - 1], br.pivots[i]);
				
				if (filtered_data[i].size() < MIN_PART_POINTS) {
					utils::log(VL_DEBUG, "Not enough data points: " + std::to_string(filtered_data[i].size()));
					continue;
				}
				explore_regression_tree(filtered_data[i], branches, result, depth+1, pconds, features, ph);
			}
		} // for all the ranges of a branch
	} // if features.size()

	// no features, or no univariate_regression
	return false;
}

		
bool analysis::split_on_pivots(const std::vector<int> pvalues, const std::string fkey, const std::vector<measure *> in, std::vector<std::vector<measure *>> &out) {
	out.resize(pvalues.size() + 1);
	for (measure *m: in) {
		int value = m->get_feature_value(fkey);
		int i = 0;
		while (i < pvalues.size() && value > pvalues[i])
			i++;
		out[i].push_back(m);
	}
	int not_empty = 0;
	for (int i = 0; i <  pvalues.size() + 1; i++) {
		if (out[i].size() > 0)
			not_empty++;
	}
	
	return (not_empty > 1);
}

