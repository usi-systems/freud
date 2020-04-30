#ifndef UTILS_HH_DEFINED
#define UTILS_HH_DEFINED

#include <iostream>
#include <string>
#include <cstring>
#include <algorithm>

enum verbosity_levels {
	VL_ERROR = 0,
	VL_QUIET,
	VL_INFO,
	VL_DEBUG
};

class utils {
private:
	static std::string log_label(enum verbosity_levels vl);

public:
	static void log(enum verbosity_levels l, std::string msg);
	static std::string uppercase(std::string str);
	static void validate_f_name(std::string &t);
	static std::string copy_validate_f_name(std::string in);
	static std::string get_function_name(std::string type);
	static std::string get_signature(std::string type);
	static unsigned long hash(unsigned char *str);
};

#endif
