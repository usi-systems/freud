#ifndef READER_HH_DEFINED
#define READER_HH

#include <vector>
#include <stack>
#include <map>
#include <string.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <limits>
#include <cstdlib>
#include <dirent.h>

#include "method.hh"
#include "utils.hh"

class reader {
private:
	static feature_type get_type(const std::string & t);
	static void read_file_binary(std::string filename, std::map<std::string, method *> &data, std::unordered_map<uint64_t, measure *> & m_ids_map, const uint32_t file_idx);

public:
	static bool read_folder(std::string folder_name, std::map<std::string, method *> &data, std::unordered_map<uint64_t, measure *> & m_ids_map);
};

#endif
