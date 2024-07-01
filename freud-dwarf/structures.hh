#ifndef STRUCTURES_HH_DEFINED
#define STRUCTURES_HH_DEFINED

#include <map>
#include <unordered_map>
#include <vector>
#include <list>
#include <set>
#include <cstdint>

#include "utils.hh"

#define MAX_DEPTH 3 
#define MAX_FEATURES 512 
#define WITH_STRUCT_IN_REG
#define CONTEXT_REGS 8
//#define USE_VECTORS_FOR_VARIADIC
#define WITH_GLOBAL_VARIABLES

// Cross information from:
// - https://github.com/libunwind/libunwind/blob/d32956507cf29d9b1a98a8bce53c78623908f4fe/src/x86_64/init.h
// - https://en.wikipedia.org/wiki/X86_calling_conventions#System_V_AMD64_ABI
// - readelf output for XMM registers
// -----------------------------------------------------------> INTs and addresses
// 0 should be RAX, return value (smaller than 64bit) in amd64
// 1 should be RDX, 2 in amd64
// 2 should be RCX, 3 in amd64
// 3 should be RBX, not used
// 4 should be RSI, 1 in amd64
// 5 should be RDI, 0 in amd64
// 6 should be RBP, 7 in my PinTool
// 7 should be RSP, 6 in my PinTool
// 8 should be R8, 4 in amd64
// 9 should be R9, 5 in amd64
// ...
// 15 should be R15, not used
// 16 should be RIP, not used
// -----------------------------------------------------------> FLOATs
// 17 is XMM0 in amd64
// ...
// 24 is XMM 7

// Here another apparently good source, with also x86 and ARM, should it ever become 
// necessary...
// https://www.imperialviolet.org/2017/01/18/cfi.html

const std::vector<int> dwarf2amd64 { -1, 2, 3, -1, 1, 0, 7, 6, 4, 5, -1, -1, -1, -1, -1, -1, -1, 8, 9, 10, 11, 12, 13, 14, 15 };

struct hierarchy_tree_node {
	hierarchy_tree_node(std::string n, std::string l): class_name(n), linkage_name(l) {
		n = std::to_string(n.size()) + n;
		unsigned char * tmp;
		tmp = (unsigned char *)n.c_str();
		uint64_t h = utils::hash(tmp);
		sym_name_hash = std::to_string(h);

		h = utils::hash((unsigned char *)l.c_str());
		sym_lname_hash = std::to_string(h);
		utils::log(VL_DEBUG, "HASH: " + class_name + " -> " + sym_name_hash); 
	};
	std::string class_name;
	std::string linkage_name;
	std::string sym_name_hash;
	std::string sym_lname_hash;
	std::set<hierarchy_tree_node *> maybe_real_type;
	std::unordered_map<std::string, int> offsets;
};

struct array_info {
	uint32_t dims;
	std::vector<uint32_t> counts;
	array_info(): dims(0) {};
};

struct member {
	std::list<std::string> feat_names;
	std::string type_name;
	std::vector<uint32_t> offset;
	std::vector<uint32_t> pointer;
	struct array_info ai;
};

struct parameter {
	std::string name;
	std::string type_name;
	std::string linkage_type_name;
	struct array_info ai;
	uint64_t location;
	int64_t offset_from_location;
	bool uses_input;
	bool in_XMM;
	bool is_ptr;
	bool is_addr;
	bool is_register_with_addr;
};

struct feature_info {
	std::string out_name;
	unsigned char num_of_values;
	feature_info() { out_name = "auto_created"; num_of_values = 0;};
	feature_info(std::string n, unsigned char f): out_name(n), num_of_values(f) {};
};

const std::unordered_map<std::string, struct feature_info> basic_features {
	{"bool",{"b",1}}, 
	{"char",{"s",1}}, {"unsigned char",{"n",1}}, {"signed char",{"n",1}}, 
	{"short int",{"n",1}}, {"short unsigned int",{"n",1}}, 
	{"int",{"n",1}}, {"unsigned int",{"n",1}}, 
	{"long int",{"n", 1}}, {"long unsigned int",{"n",1}}, 
	{"long long int",{"n",1}}, {"long long unsigned int",{"n",1}}, {"size_t",{"n",1}},
	{"float",{"n",1}}, {"double",{"n",1}},

	// ARRAYS
	{"array bool",{"a",2}}, 
	{"array char",{"a",2}}, {"array unsigned char",{"a",2}}, {"array signed char",{"a",2}}, 
	{"array int", {"a", 2}}, {"array unsigned int", {"a", 2}},
	{"array long int",{"a", 2}}, {"array long unsigned int",{"a", 2}},
	{"array short int", {"a", 2}}, {"array short unsigned int", {"a", 2}},
	{"array long long int",{"a",2}}, {"array long long unsigned int",{"a",2}}, {"array size_t",{"a",2}},
	{"array float",{"a",2}}, {"array double",{"a",2}},
	
	// ENUMS
	{"enum int",{"e",1}}, {"enum unsigned int",{"e",1}}, {"enum unsigned char",{"e",1}},
	{"enum long",{"e",1}}, {"enum unsigned long",{"e",1}},
	
};


#endif
