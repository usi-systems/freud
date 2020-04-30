#ifndef CHECKER_HH_DEFINED
#define CHECKER_HH_DEFINED

#include "method.hh"
#include "stats.hh"
#include "utils.hh"

#include <fstream>


class checker {
private:
	metric_type metric;
	method * mtd;
	annotation * pa;
	std::ofstream * out_file;
	void filter_data(corr_with_constraints cwc, std::vector<measure *> in, std::vector<measure *> &out);

public:
	checker(std::string mname, method * m, metric_type mtype, bool remove_dependencies, annotation * performance_annotation, std::ofstream * out);

	bool validate_annotation();

};


#endif
