#include "utils.hh"

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

std::string utils::uppercase(std::string str) {
	std::string res = "";
	for (auto c: str) res += toupper(c);
	return res;
}

void utils::validate_f_name(std::string &t) {
	std::replace(t.begin(), t.end(), '~', '_');
	std::replace(t.begin(), t.end(), '.', '_');
	std::replace(t.begin(), t.end(), '-', '_');
	std::replace(t.begin(), t.end(), '&', '_');
	std::replace(t.begin(), t.end(), '\'', '_');
	std::replace(t.begin(), t.end(), ')', '_');
	std::replace(t.begin(), t.end(), '(', '_');
	std::replace(t.begin(), t.end(), '*', '_');
	std::replace(t.begin(), t.end(), ':', '_');
	std::replace(t.begin(), t.end(), ',', '_');
	std::replace(t.begin(), t.end(), '>', '_');
	std::replace(t.begin(), t.end(), '<', '_');
	std::replace(t.begin(), t.end(), ' ', '_');
	std::replace(t.begin(), t.end(), '[', '_');
	std::replace(t.begin(), t.end(), ']', '_');
	std::replace(t.begin(), t.end(), '=', '_');
	std::replace(t.begin(), t.end(), ';', '_');
}

std::string utils::copy_validate_f_name(std::string in) {
	std::string res = in;
	validate_f_name(res);
	return res;
}

std::string utils::get_function_name(std::string type) {
	std::string fname = type;
	validate_f_name(fname);
	fname = "get_" + fname + "_features";
	return fname;
}

std::string utils::get_signature(std::string type) {
	std::string sig = type;
	sig = "void " + get_function_name(type) + "(" + type + " value, double *values);\n\n";
	sig += "void " + get_function_name(type) + "_ptr(void * value, double *values);\n\n";
	return sig;
}

unsigned long utils::hash(unsigned char *str) {
	unsigned long hash = 5381;
	int c;

	while (c = *str++)
	    hash = ((hash << 5) + hash) + c; 

	return hash;
}

