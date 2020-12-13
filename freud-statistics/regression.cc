#include "regression.hh"
#include <iostream>
#include <iomanip>
#include <limits>
#include <sstream>

multiple_regression::multiple_regression(std::stringstream &ss) {
	std::string temp;
	ss >> temp; // [det=
	ss >> determination;
	ss >> temp; // ]

	// Coefficients
	while (ss) {
		ss >> temp;
		if (!ss)
			break;
		int star_index = temp.find("*");
		if (star_index == std::string::npos) {
			// Intercept
			intercept_value = std::stod(temp);
		} else {
			std::string c = temp.substr(0, star_index);
			std::string fname = temp.substr(star_index + 1, std::string::npos);
			coefficients[fname] = std::stod(c);
			features.push_back(fname);
		}
	}
}

multiple_regression::multiple_regression(const double r2, const double intercept, const std::vector<std::string> &in_features, const std::vector<std::string> &var_names, const std::vector<int> &degrees, const std::vector<std::vector<double>> &coefficients, const std::unordered_map<std::string, double> &coefficients_map, const std::unordered_map<std::string, std::string> &vnames) {
	determination = r2;
	intercept_value = intercept;
	this->coefficients = coefficients_map;
	this->var_names = vnames;
	this->features = in_features;
	int nr = features.size();
}


void multiple_regression::print(std::ostream & stream) {
	stream << "RR ";
	stream << intercept_value;
	stream << " + ";
#if 0
	for (std::pair<std::string, univariate_regression *> p: feature_regressions) {
		std::string fname = p.first;
		univariate_regression * r = p.second;
		stream << " {" << fname << "_" << p.second->var_name << "} ";
		for (int i = 0; i < r->degree; i++) {
			stream << r->coefficients[i];
			stream << " ";
		}
	}
#endif
	stream << "; det: " << determination;
}

std::string multiple_regression::get_string() {
	std::ostringstream res;
	res.precision(std::numeric_limits<long double>::digits10 + 1);
	res << std::scientific;
	
	res << "[det= " << determination << " ] ";
	res << intercept_value;
	for (std::string f: features) {
		if (coefficients[f] >= 0)
			res << " +" << coefficients[f] << "*" << var_names[f];
		else
			res << " " << coefficients[f] << "*" << var_names[f];
	}
    return res.str();
}
