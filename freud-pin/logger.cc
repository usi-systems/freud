enum verbosity_levels {
	VL_ERROR = 0,
	VL_QUIET,
	VL_INFO,
	VL_DEBUG
};

enum verbosity_levels vl = VL_DEBUG;

std::string log_label(enum verbosity_levels vl) {
	if (vl == VL_ERROR) {
		return "ERR: ";
	} else if (vl == VL_INFO) {
		return "INFO: ";
	} else if (vl == VL_DEBUG) {
		return "DBG: ";
	}
	return "UNK: ";
}

void log(enum verbosity_levels l, const std::string msg) {
	if (l <= vl)
		std::cout << log_label(l) << msg << std::endl;
}


