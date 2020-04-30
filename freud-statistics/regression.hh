#ifndef REGRESSION_HH_DEFINED
#define REGRESSION_HH_DEFINED

#include "const.hh"

#include <ostream>
#include <string>
#include <sstream>
#include <iostream>

#include <vector>
#include <unordered_set>
#include <unordered_map>

class multiple_regression {
public:
	// constructors
	multiple_regression(const double r2, const double intercept, const std::vector<std::string> &in_features, const std::vector<std::string> &var_names, const std::vector<int> &degrees, const std::vector<std::vector<double>> &coefficients, const std::unordered_map<std::string, double> &coefficients_map, const std::unordered_map<std::string, std::string> &vnames);

	// parse from string
	multiple_regression(std::stringstream &in);

	// R^2
	double determination;
	double intercept_value;
	std::vector<std::string> features;
	std::unordered_map<std::string, double> coefficients;
	std::unordered_map<std::string, std::string> var_names;
	
	void print(std::ostream & stream);
	std::string get_string();
};

#endif
