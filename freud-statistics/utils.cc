#include "utils.hh"
#include "stats.hh"
#include <algorithm>

#include <sstream>

enum verbosity_levels vl = VL_INFO;

std::string utils::log_label(enum verbosity_levels vl) {
	if (vl == VL_ERROR) {
		return "ERR";
	} else if (vl == VL_INFO) {
		return "INFO: ";
	} else if (vl == VL_DEBUG) {
		return "DBG: ";
	}
	return "UNK: ";
}

void utils::log(enum verbosity_levels l, const std::string msg) {
	if (l <= vl)
		std::cout << log_label(l) << msg << std::endl;
}

std::string utils::base_feature(std::string feature) {
	int colon_pos = feature.find(":");
	if (colon_pos == std::string::npos) {
		int at_idx = feature.find("^");
		if (at_idx == std::string::npos)
			return feature;
		else 
			return feature.substr(0, at_idx);
	} else {
		return feature;
	}
}

int utils::get_degree(std::string feat) {
	int at_idx = feat.find("^");
	if (at_idx == std::string::npos)
		return 1;
	else {
		at_idx++;
		int cnt = 0;
		while (at_idx + cnt < feat.size() && std::isdigit(feat[at_idx + cnt]))
			cnt++;
		return std::stoi(feat.substr(at_idx, cnt));
	} 
	return 0;
}


std::string utils::get_random_color(int seed) {
	int r,g,b;
	if (seed == 0) {
		r = 187 + rand() % 68;
		b = rand() % 187;
		g = rand() % 187;
	} else if (seed == 1) {
		g = 187 + rand() % 68;
		b = rand() % 187;
		r = rand() % 187;
	} else if (seed == 2) {
		b = 187 + rand() % 68;
		g = rand() % 187;
		r = rand() % 187;
	} else {
		r = g = b = rand() % 200;
	}
	std::ostringstream ss;
	ss << std::hex << r << g << b; 
	std::string res (ss.str());
	return res;
}

bool utils::similar_cwc(struct corr_with_constraints cwc1, struct corr_with_constraints cwc2) {
	// TODO: Consider path conditions...
	multiple_regression *r1 = cwc1.reg, *r2 = cwc2.reg;

	/*
	 * TODO: reenable this
	if (r1 != nullptr && r2 != nullptr) {
		if (r1->degree != r2->degree)
			return false;
		for (int c = 0; c < r1->degree; c++) {
			double c1 = r1->coefficients[c];
			double c2 = r2->coefficients[c];
			double diff = c1 / c2;
			if (diff < 0.9 || diff > 1.1)
				return false;

			return true;
		}
	}
	*/

	return false;
}

bool utils::similar_regressions(std::vector<struct corr_with_constraints> vr1, std::vector<struct corr_with_constraints> vr2) {
	if (vr1.size() != vr2.size())
		return false; // Maybe over-restrictive?

	bool similar = true;
	int nreg = vr1.size();
	// TODO: this check should not depend on the order of the regressions
	for (int r = 0; r < nreg && similar; r++) {
		similar &= similar_cwc(vr1[r], vr2[r]);
	}

	return similar;
}

void utils::get_filtered_data(corr_with_constraints cwc, const std::vector<measure *> in, std::vector<measure *> &out) {
	out.clear();
	for (measure *m: in) {
		bool good = true;
		for (struct path_filter pf: cwc.path_conditions_vec) {
			int value = m->get_feature_value(pf.feature);
			switch (pf.flt) {
				case FLT_LE:
					if (value > pf.upper_bound)
						good = false;
					break;

				case FLT_GT:
					if (value <= pf.lower_bound)
						good = false;
					break;
				
				case FLT_BT:
					if (value > pf.upper_bound || value <= pf.lower_bound)
						good = false;
					break;

				case FLT_EQ:
					if (value != pf.lower_bound)
						good = false;
					break;

				default:
					break;
			}
			if (!good)
				break;
		}

		if (good)
			out.push_back(m);
	}
}

double ctrv;
int mmetric;

    struct {
        bool operator()(measure *a, measure *b) const
        {   
			double am = a->measures[mmetric];
			double bm = b->measures[mmetric];

			// closest to the centroid
            return (abs(am - ctrv)) < (abs(bm - ctrv));
			
			// minimum
			//return am < bm;
        }   
    } dist_from_ctr;


void utils::remove_addnoise(std::string fkey, metric_type metric, const std::vector<measure *> data, std::vector<measure *> &res) {
	res.clear();
	srand(time(NULL));
	// build the set of x values observed for this method
	std::set<int> xvalues;
	for (measure *m: data) {
		xvalues.insert(m->get_feature_value(fkey));
	}
	std::cout << "Removing anois for key " << fkey << " with diff. val.:" << xvalues.size() << std::endl;
	for (int x: xvalues) {
		//std::cout << "X = " << x << std::endl;
		// build the vector of y values for this x value
		std::vector<measure *> ys;
		
		measure * min_m = nullptr;
		uint64_t min_v = UINT64_MAX;

		for (measure *m: data) {
			if (m->get_feature_value(fkey) == x) {
				ys.push_back(m);
				if (m->measures[metric] < min_v) {
					min_v = m->measures[metric];
					min_m = m;
				}
			}
		}
#if 1
		//take only the minimum
		if (min_m) {
			res.push_back(min_m);
			//std::cout << "Taking (" << x << ", " << min_v << ")" << std::endl;  
		}
#else 
		//clustering
		if (ys.size() < 10) { 
			// no point in clustering
			res.insert(res.end(), ys.begin(), ys.end()); //push_back(ys[0]);
			continue;
		}
		std::vector<struct cluster> clusters;
		stats::kde_clustering(ys, metric, clusters, true);
		std::cout << "Clusters = " << clusters.size() << std::endl;
		if (clusters.size() > 0) {
#if 0
			// Consider all the meaningful clusters
			for (struct cluster c: clusters) {
				int num_of_samples = c.probability * 10;
				// I know I have enough samples
				std::cout << "c " << c.centroid << " | Samples = " << num_of_samples << std::endl;
				// sort based on the distance from centroid
				mmetric = (int)metric;
				ctrv = (uint32_t)c.centroid;
				std::vector<measure *> sorted_data;
				sorted_data.resize(data.size());
				memcpy(sorted_data.data(), data.data(), sizeof(measure *) * data.size());
				std::sort(sorted_data.begin(), sorted_data.end(), dist_from_ctr);
				for (int i = 0; i < num_of_samples;) {
					measure *m = sorted_data[i];
					res.push_back(m);
					i++;
					std::cout << "(" << x << ", " << m->measures[metric] << ")" << std::endl;
					/*
					uint32_t mvalue = m->measures[metric];
					if (mvalue > c.lower_bound && mvalue <= c.upper_bound) {
						i++;
						res.push_back(m);
					}
					*/
				}
			}
#else
			// Consider only the most important cluster
			int maxp = 0;
			for (struct cluster c: clusters) {
				if (c.probability > maxp) {
					ctrv = c.centroid;
					maxp = c.probability;
				}
			}
			int num_of_samples = maxp * 10;
			// I know I have enough samples
			std::cout << "c " << ctrv << " | Samples = " << num_of_samples << std::endl;
			// sort based on the distance from centroid
			mmetric = (int)metric;
			std::vector<measure *> sorted_data;
			sorted_data.resize(data.size());
			memcpy(sorted_data.data(), data.data(), sizeof(measure *) * data.size());
			std::sort(sorted_data.begin(), sorted_data.end(), dist_from_ctr);
			for (int i = 0; i < num_of_samples;) {
				measure *m = sorted_data[i];
				res.push_back(m);
				i++;
				std::cout << "(" << x << ", " << m->measures[metric] << ")" << std::endl;
				/*
				uint32_t mvalue = m->measures[metric];
				if (mvalue > c.lower_bound && mvalue <= c.upper_bound) {
					i++;
					res.push_back(m);
				}
				*/
			}

#endif
		} // clusters.size() > 0
		else {
			// No clusters found, just pick some points randomly!
			int num_of_samples = 10;
			for (int i = 0; i < num_of_samples; i++) {
				measure *m = data[rand() % data.size()];
				res.push_back(m);
			}
		}
#endif
	}
}

void utils::get_main_trend(std::string fkey, metric_type metric, const std::vector<measure *> data, std::vector<measure *> &res) {
	res.clear();
	srand(time(NULL));
	// build the set of x values observed for this method
	std::set<int> xvalues;	
	for (measure *m: data) {
		xvalues.insert(m->get_feature_value(fkey));
	}
	//std::cout << "Getting main trend for key " << fkey << " with diff. val.:" << xvalues.size() << std::endl;
	for (int x: xvalues) {
		//std::cout << "--- X = " << x << " ---" << std::endl;
		// build the vector of y values for this x value
		std::vector<measure *> ys;
		
		measure * min_m = nullptr;
		uint64_t min_v = UINT64_MAX;

		for (measure *m: data) {
			if (m->get_feature_value(fkey) == x) {
				ys.push_back(m);
				if (m->measures[metric] < min_v) {
					min_v = m->measures[metric];
					min_m = m;
				}
			}
		}
#if 0
		//take only the minimum
		if (min_m)
			res.push_back(min_m);
#else 
		//clustering
		if (ys.size() < 10) { 
			// no point in clustering
			res.insert(res.end(), ys.begin(), ys.end()); //push_back(ys[0]);
			continue;
		}
		std::vector<struct cluster> clusters;
		stats::kde_clustering(ys, metric, clusters, true);
		//std::cout << "Clusters = " << clusters.size() << std::endl;
		if (clusters.size() > 0) {
#if 0
			// Consider all the meaningful clusters
			for (struct cluster c: clusters) {
				int num_of_samples = c.probability * 10;
				// I know I have enough samples
				std::cout << "c " << c.centroid << " | Samples = " << num_of_samples << std::endl;
				// sort based on the distance from centroid
				mmetric = (int)metric;
				ctrv = (uint32_t)c.centroid;
				std::vector<measure *> sorted_data;
				sorted_data.resize(data.size());
				memcpy(sorted_data.data(), data.data(), sizeof(measure *) * data.size());
				std::sort(sorted_data.begin(), sorted_data.end(), dist_from_ctr);
				for (int i = 0; i < num_of_samples;) {
					measure *m = sorted_data[i];
					res.push_back(m);
					i++;
					std::cout << "(" << x << ", " << m->measures[metric] << ")" << std::endl;
					/*
					uint32_t mvalue = m->measures[metric];
					if (mvalue > c.lower_bound && mvalue <= c.upper_bound) {
						i++;
						res.push_back(m);
					}
					*/
				}
			}
#else
			// Consider only the most important cluster
			double maxp = 0;
			for (struct cluster c: clusters) {
				//std::cout << "(dbg) c " << c.centroid << ", " << c.lower_bound << ", " << c.upper_bound << " | p = " << c.probability << std::endl;

				if (c.probability > maxp) {
					ctrv = c.centroid;
					maxp = c.probability;
				}
			}
			int num_of_samples = maxp * 10;//ys.size();
			// I know I have enough samples
			//std::cout << "c " << ctrv << " | Samples = " << num_of_samples << std::endl;
			// sort based on the distance from centroid
			mmetric = (int)metric;
			std::vector<measure *> sorted_data;
			for (measure *n: data)
				if (n->get_feature_value(fkey) == x)
					sorted_data.push_back(n);
			std::sort(sorted_data.begin(), sorted_data.end(), dist_from_ctr);
			for (int i = 0; i < num_of_samples;) {
				measure *m = sorted_data[i];
				res.push_back(m);
				i++;
				//std::cout << "  - (" << m->get_feature_value(fkey) << ", " << m->measures[metric] << ")" << std::endl;
				/*
				uint32_t mvalue = m->measures[metric];
				if (mvalue > c.lower_bound && mvalue <= c.upper_bound) {
					i++;
					res.push_back(m);
				}
				*/
			}

#endif
		} // clusters.size() > 0
		else {
			// No clusters found, just pick some points randomly!
			int num_of_samples = 10;
			for (int i = 0; i < num_of_samples; i++) {
				measure *m = data[rand() % data.size()];
				res.push_back(m);
			}
		}
#endif
	}
}


std::string utils::generate_annotation_string(const corr_with_constraints cwc) {
	std::string result = "R ";
	// Main trend
	if (cwc.main_trend)
		result += "M ";
	// Lower boundary
	else if (cwc.addnoise_removed)
		result += "* ";
	// Complete
	else
		result += "C ";
	if (cwc.path_conditions_vec.size()) {
		result += "[ ";
		bool first = true;
		for (struct path_filter pf: cwc.path_conditions_vec) {
			if (!first)
				result += ", ";
			result += pf.feature;
			switch (pf.flt) {
				case FLT_LE:
					result += " <= " + std::to_string(pf.upper_bound);
					break;

				case FLT_GT:
					result += " > " + std::to_string(pf.lower_bound);
					break;
				
				case FLT_BT:
					result += " >| " + std::to_string(pf.lower_bound)
						+ " | <= " + std::to_string(pf.upper_bound);
					break;
				
				case FLT_EQ:
					if (pf.feature == "CL")
						result += " == " + std::to_string(pf.probability) + " " + std::to_string(pf.cl_lower_bound) + " " + std::to_string(pf.cl_upper_bound);
					else
						result += " == " + std::to_string(pf.lower_bound);
					break;

				default:
					break;
			}
			first = false;
		}
		result += " ] ";
	} else {
		result += "[ * ] ";
	}
	if (cwc.metric == MT_MEM) {
		result += "M ~ ";
	} else if (cwc.metric == MT_TIME) {
		result += "T ~ ";
	} else if (cwc.metric == MT_LOCK) {
		result += "L ~ ";
	} else if (cwc.metric == MT_WAIT) {
		result += "W ~ ";
	} else if (cwc.metric == MT_PAGEFAULT_MINOR) {
		result += "p ~ ";
	} else if (cwc.metric == MT_PAGEFAULT_MAJOR) {
		result += "P ~ ";
	}
	if (cwc.constant) {
		result += "[det= 1.0 ] 0 0 foo 0 " + std::to_string(cwc.constant_value);
		/* THESE UNITS ARE NOW IMPLICIT
		if (cwc.metric == MT_MEM)
			result += "B";
		else
			result += "us";
		*/
	}
	else if (cwc.reg)
		result += cwc.reg->get_string();
	else
		result += "???";
	result += "\n";
	return result;
}

std::string utils::generate_annotation_string(const struct annotation * pa) {
	if (pa->regressions.size())
		return generate_annotation_string(&(pa->regressions));
	else
		return generate_annotation_string(&(pa->clusters));
}

std::string utils::generate_annotation_string(const std::vector<struct cluster> * clusters) {
	std::string result = "";
	for (int cc = 0; cc < clusters->size(); cc++) {
		result += (*clusters)[cc].get_string();
	}
	return result;
}

std::string utils::generate_annotation_string(const std::vector<corr_with_constraints> * in) {
	std::string result = "";
	for (corr_with_constraints cwc: *in) 
		result += generate_annotation_string(cwc);
	return result;
}

bool utils::comp_contains(const std::string &comp, const std::string &f) {
	int colon_pos = comp.find(":"); 
	if (colon_pos != std::string::npos) {
		std::string f1 = comp.substr(0, colon_pos);
		std::string f2 = comp.substr(colon_pos+1, std::string::npos);
		return (f == f1 || f == f2);
	} else {
		return false;
	}
}

