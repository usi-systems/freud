#ifndef PLOTTER_HH_DEFINED
#define PLOTTER_HH_DEFINED

#include <string>
#include <vector>
#include "method.hh"

#define OF_INTERVALS 3

class plotter {
private:
	static std::string getGnuplotScriptContent(const std::vector<measure *> & data, std::string fname, const std::string &main_feat, const std::vector<std::string> &magic, metric_type mtype, double radius, corr_with_constraints cwc, std::string func_name, std::string data_name);

	static void plot_annotation(method * mtd, corr_with_constraints cwc, metric_type metric);

public:
	static bool graph(std::vector<measure *> data, std::string name, std::vector<std::string> & fkeys, corr_with_constraints cwc, metric_type metric);

	static bool graph(method * mtd, std::string name, std::vector<std::string> & fkeys, metric_type metric);

	static void plot_annotations(method * mtd, metric_type metric);
};

#endif
