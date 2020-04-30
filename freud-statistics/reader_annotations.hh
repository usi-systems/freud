#ifndef READER_ANNOTATIONS_HH_DEFINED
#define READER_ANNOTATIONS_HH

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


class reader_annotations {
private:

public:
	static void read_annotations_file(std::string filename, std::string symbol_name, std::map<std::string, annotation *> &data);
	static bool read_annotations_folder(std::string folder_name, std::map<std::string, annotation *> &data);
};

#endif

