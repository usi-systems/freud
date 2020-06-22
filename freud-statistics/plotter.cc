#include "plotter.hh"
#include "stats.hh"
#include "utils.hh"
#include <fstream>
#include <sstream>

bool plotter::graph(method * mtd, std::string name, std::vector<std::string> & fkeys, metric_type metric) {
	// This method is used for the command line parameter GNUPLOT,
	// so we do not need to draw any univariate_regression here. Create a fake one 
	struct corr_with_constraints foo;
	foo.reg = 0;
	foo.constant = false;
	graph(mtd->data, name, fkeys, foo, metric);
	return true;
}

bool plotter::graph(std::vector<measure *> data, std::string name, std::vector<std::string> & fkeys, corr_with_constraints cwc, metric_type metric) {
	std::string gp_code = "";
	double min, max;
	
	std::vector<std::string> relevant_feat;
	for (std::string f: fkeys)
		if (cwc.reg && cwc.reg->coefficients.find(f) != cwc.reg->coefficients.end())
			if (f.find(":") == std::string::npos)
				relevant_feat.push_back(f);

	if (cwc.reg)
		stats::printGNUPlotData(cwc.data_points, relevant_feat, metric, false, gp_code, min, max);
	else
		stats::printGNUPlotData(data, fkeys, metric, false, gp_code, min, max);
	std::string func_name = name.substr(strlen("eps/"), std::string::npos);
	std::string data_fname = "plots/gnuplot" + func_name + fkeys[0] + " , " + fkeys[1] + ".dat";
	std::ofstream gnup(data_fname);
	gnup << gp_code;
	gnup.close();

	if (!cwc.reg) {
		std::string script_name = "plots/pscript" + func_name + "command.gp";
		std::ofstream script(script_name);
		double radius = (max - min) / 5000000000;
		script << getGnuplotScriptContent(cwc.data_points, name, "command", relevant_feat, metric, radius, cwc, func_name, data_fname);
		script.close();
		std::string command = "gnuplot \"" + script_name + "\"";
		int s = system(command.c_str());
		if (s)
			std::cout << "Something went wrong with gnuplot: " << std::to_string(s) << std::endl;
		return true;
	}

	for (std::string fr: cwc.reg->features) {
		int colon_pos = fr.find(":");
		// I do not want to plot the interaction term as a feature...
		// Unless I have only those...
		if (colon_pos != std::string::npos)
			continue;
		std::string bf = utils::base_feature(fr);
		std::string script_name = "plots/pscript" + func_name + bf + ".gp";
		std::ofstream script(script_name);
		double radius = (max - min) / 5000000000;
		script << getGnuplotScriptContent(cwc.data_points, name, bf, relevant_feat, metric, radius, cwc, func_name, data_fname);
		script.close();
		std::string command = "gnuplot \"" + script_name + "\"";
		int s = system(command.c_str());
		if (s)
			std::cout << "Something went wrong with gnuplot: " << std::to_string(s) << std::endl;
	}
	return true;
}

std::string plotter::getGnuplotScriptContent(const std::vector<measure *> & data, std::string fname, const std::string &main_feat, const std::vector<std::string> &magic, metric_type mtype, double radius, corr_with_constraints cwc, std::string func_name, std::string data_name) {
	int feat_col = 0, metric_col;
	for (; feat_col < magic.size(); feat_col++) {
		//std::cout << "DBGFEAT " << magic[feat_col] << std::endl;
		if (utils::base_feature(magic[feat_col]) == main_feat)
			break;
	}
	feat_col++; // Numbering in gluplot start from 1
	metric_col = magic.size() + 1;
	// Longer strings are a problem for gnuplot
	if (fname.size() > 50)
		fname = fname.substr(0, 50);
	std::ostringstream res;
	res.precision(std::numeric_limits<long double>::digits10 + 1);
	std::string title;
	// skip "eps/" from the filename
	for (int i = 4; i < fname.size(); i++) {
		if (fname[i] == '_') {
			title.append("\\\\_");
		} else {
			title.append(fname.c_str() + i, 1);
		}
	}
	res << "set title \"" << title << "\"\n";
	if (data.size()) {
		std::string x_title;
		std::string tmp_title = main_feat;
		for (int i = 0; i < tmp_title.size(); i++) {
			if (tmp_title[i] == '_') {
				x_title.append("\\\\_");
			} else {
				x_title.append(tmp_title.c_str() + i, 1);
			}
		}
		res << "set xlabel \"" << x_title << "\" font \"Helvetica,9\"\n";
	}
	else {
		res << "set xlabel \"manual\" font \"Helvetica,9\"\n";
	}
	res << "set terminal eps enhance color font \"Helvetica,7\" size 3.2,2\n";
	res << "set style circle\n";// radius " + std::to_string(radius) + "\n";
	res << "set style fill  transparent solid 0.35 noborder\n";
	
	if (mtype == MT_MEM) {
		res << "set ylabel \"Memory (bytes)\" font \"Helvetica,9\"\n";
		res << "set output \"" << fname << main_feat << "_mem.eps\"\n";
	} else if (mtype == MT_TIME) {
		res << "set ylabel \"Time (usecs)\" font \"Helvetica,9\"\n";
		res << "set output \"" << fname << main_feat << "_time.eps\"\n";
	} else if (mtype == MT_LOCK) {
		res << "set ylabel \"Lock Time (usecs)\" font \"Helvetica,9\"\n";
		res << "set output \"" << fname << main_feat << "_lock.eps\"\n";
	} else if (mtype == MT_WAIT) {
		res << "set ylabel \"Wait Time (usecs)\" font \"Helvetica,9\"\n";
		res << "set output \"" << fname << main_feat << "_lock.eps\"\n";
	} else if (mtype == MT_PAGEFAULT_MINOR) {
		res << "set ylabel \"Minor Page Faults (#)\" font \"Helvetica,9\"\n";
		res << "set output \"" << fname << main_feat << "_mpf.eps\"\n";
	} else if (mtype == MT_PAGEFAULT_MAJOR) {
		res << "set ylabel \"Major Page Faults (#)\" font \"Helvetica,9\"\n";
		res << "set output \"" << fname << main_feat << "_Mpf.eps\"\n";
	} else {
		res << "set ylabel \"Unknown\" font \"Helvetica,9\"\n";
		res << "set output \"" << fname << main_feat << "_unknown.eps\"\n";
	}

	int fi = 0, mfi = 0;
	double intercept = 0;
	std::unordered_map<int, std::string> foolish_map;
	if (cwc.reg) {
		intercept = cwc.reg->intercept_value;
		// Draw the multiple regression, one dimension at a time!
		multiple_regression * reg = cwc.reg;
		// Here I will have n regressions, with a different intercept value
		// based on the values of the other features
		for (std::string fr: cwc.reg->features) {
			bool interaction_term = false;
			int ff_deg = 0, sf_deg = 0;
			std::string bf = utils::base_feature(fr);
			if (bf == main_feat) {
				res << "reg" << mfi++ << "(x) = ";
				res << cwc.reg->coefficients[fr];
				ff_deg = utils::get_degree(fr);
				if (ff_deg != LOG_DEG)
					for (int i = 0; i < ff_deg; i++) 
						res << "*x";
				else 
					res << "*x*log(x)/log(2)";
			} else {
				int colon_pos = fr.find(":");
				if (colon_pos != std::string::npos) {
					// Only pairwise interactions, atm
					res << "oreg" << fi << "(x,y) = ";
					interaction_term = true;
					ff_deg = utils::get_degree(fr.substr(0, colon_pos));
					sf_deg = utils::get_degree(fr.substr(colon_pos+1, std::string::npos));
					res << cwc.reg->coefficients[fr];
					if (ff_deg != LOG_DEG)
						for (int i = 0; i < ff_deg; i++) 
							res << "*x";
					else 
						res << "*x*log(x)/log(2)";
					if (sf_deg != LOG_DEG)
						for (int i = 0; i < sf_deg; i++) 
							res << "*y";
					else 
						res << "*y*log(y)/log(2)";
				} else {
					res << "oreg" << fi << "(x) = ";
					ff_deg = utils::get_degree(fr);
					res << cwc.reg->coefficients[fr];
					if (ff_deg != LOG_DEG)
						for (int i = 0; i < ff_deg; i++) 
							res << "*x";
					else 
						res << "*x*log(x)/log(2)";
				}
				foolish_map[fi] = fr;
				fi++;
			}
			res << "\n";
		}
	}


	if (fi) {
		// I have more than 1 dimension, plot also the second dimension for some values
		res << "plot \"" << data_name << "\" using " << std::to_string(feat_col) << ":" << std::to_string(metric_col) << " with circles lc rgb \"blue\" t \"\"";
		
		for (int of = 0; of < fi; of++) {
			std::string ofeat = foolish_map[of];
			//std::cout << "OFEAT " << ofeat << std::endl;

			// I do not want to draw anything directly for the interaction term
			int colon_pos = ofeat.find(":");
			bool is_interaction_term = (colon_pos != std::string::npos); 
			if (is_interaction_term) {
				continue;
			}
	
			std::vector<double> svalues;
			//std::cout << "Getting intervals for " << ofeat << std::endl;
			stats::get_ofeature_intervals(cwc.data_points, utils::base_feature(ofeat), svalues, OF_INTERVALS);
			//std::cout << "Done for " << ofeat << std::endl;
			for (int i = 0; i < OF_INTERVALS; i++) {
				if (!is_interaction_term) {
					res << ", oreg" << std::to_string(of) << "(" << std::to_string(svalues[i]) << ")";
				} else {
					res << ", oreg" << std::to_string(of) << "(x," << std::to_string(svalues[i]) << ")";
				}
				// PLUS THE AVERAGE FOR THE OTHER FEATURES
				for (int off = 0; off < fi; off++) {
					if (off == of)
						continue;

					std::string offeat = foolish_map[off];
					//std::cout << "OFF is " << offeat << std::endl;
					int off_colon_pos = offeat.find(":");
					bool off_is_interaction_term = (off_colon_pos != std::string::npos); 

					if (off_is_interaction_term) {

						// I only want to consider the "other" feature of the interaction
						// term. I.e., the one that is not the main feature
						std::string f1 = offeat.substr(0, off_colon_pos);
						std::string f2 = offeat.substr(off_colon_pos+1, std::string::npos);
						res << "+ oreg" << std::to_string(off) << "(";

						// If the other feature is an interaction term
						if (utils::base_feature(f1) == main_feat)
							res << "x,";
						else if (utils::base_feature(f1) == utils::base_feature(ofeat))
							 res << std::to_string(svalues[i]) << ",";
						else 
							 res << std::to_string(stats::get_avg_value(cwc.data_points,utils::base_feature(f1)))  << ",";
						
						if (utils::base_feature(f2) == main_feat)
							res << "x";
						else if (utils::base_feature(f2) == utils::base_feature(ofeat))
							 res << std::to_string(svalues[i]);
						else 
							 res << std::to_string(stats::get_avg_value(cwc.data_points,utils::base_feature(f2)));
						
						res << ")";
					} else {
						res << "+ oreg" << std::to_string(off) << "(" << std::to_string(stats::get_avg_value(cwc.data_points, foolish_map[off])) << ")";
					}
				}
				if (data.size()) {
					std::string of_title;
					std::string tmp_title = ofeat;
					for (int i = 0; i < tmp_title.size(); i++) {
						if (tmp_title[i] == '_') {
							of_title.append("\\\\_");
						} else {
							of_title.append(tmp_title.c_str() + i, 1);
						}
					}
					for (int m = 0; m < mfi; m++)
						res << "+ " << std::to_string(intercept) << " + reg" + std::to_string(m) + "(x) "; 
					res << "title \"" << of_title << "[" << std::to_string((long)svalues[i]) << "]\" with lines lw 5 lc rgb \"#" << utils::get_random_color(of) << "\"";
				}
				else {
					for (int m = 0; m < mfi; m++)
						res << "+ " << std::to_string(intercept) << " + reg" + std::to_string(m) + "(x) ";
					res << "title \" custom\" with lines lw 5 lc rgb \"#" << utils::get_random_color(of) << "\"";
				}
			}
		}
	}
	else {
		// Only one feature, easy peasy
		res << "plot \"" << data_name << "\" using " << std::to_string(feat_col) << ":" << std::to_string(metric_col) << " with circles lw 5 lc rgb \"blue\" t \"\"";
		if (cwc.reg) {
			res << ", + " << std::to_string(intercept);
		for (int m = 0; m < mfi; m++)\
			 res << " + reg" << std::to_string(m) << "(x) ";
		}
		res <<  "notitle with lines lw 5 lc rgb \"red\"";
	}
	res << "\n";
	return res.str();
}

void plotter::plot_annotations(method * mtd, metric_type metric) {
	for (corr_with_constraints cwc: mtd->performance_annotation.regressions)
		plot_annotation(mtd, cwc, metric);
}

void plotter::plot_annotation(method * mtd, corr_with_constraints cwc, metric_type metric) {
	if (cwc.reg == nullptr) {
		return;
	}
	std::string name = mtd->name;
	for (struct path_filter pf: cwc.path_conditions_vec) {
		name += pf.feature;
		switch (pf.flt) {
			case FLT_LE:
				name += "LE" + std::to_string(pf.upper_bound);
				break;

			case FLT_GT:
				name += "GT" + std::to_string(pf.lower_bound);
				break;
			
			case FLT_BT:
				name += "BT" + std::to_string(pf.lower_bound)
					+ "-" + std::to_string(pf.upper_bound);
				break;
			case FLT_EQ:
				if (pf.feature == "CL")
					name += "EQ" + std::to_string(pf.probability);
				else
					name += "EQ" + std::to_string(pf.lower_bound);
				break;
			default:
				break;
		}
	}
	
	std::vector<std::string> feature;
	for (std::string f: cwc.reg->features)
		feature.push_back(f); 
	//feature.push_back(cwc.reg->var_name);
	feature.push_back("no_vname_yet");
	
	std::vector<measure *> tmp_data;	
	utils::get_filtered_data(cwc, mtd->data, tmp_data);
	if (cwc.main_trend)
		name += "MTR";
	else if (cwc.addnoise_removed)
		name += "WON";
	plotter::graph(tmp_data, "eps/" + name, feature, cwc, metric);
	
	tmp_data.clear();
}

