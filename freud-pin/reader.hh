#ifndef READER_HH_INCLUDED
#define READER_HH_INCLUDED

#include "rtn_descriptor.h"

class bin_reader {
public:
	static void read(std::string fname, std::unordered_map<std::string, struct routine_descriptor> &rmap); 	
};

#endif
