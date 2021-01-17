#ifndef RTN_STRUCTURE_H_
#define RTN_STRUCTURE_H_

#include <string>
#include <map>
#include <vector>

#include "rtn_execution.h"

#define EXPECTED_SAMPLES_PER_PERIOD 1000.0 // num of logs to collect for a specific object
#define MAXTHREADS 64

#define SYSTEM_FEATURES_CNT 1

unsigned int DUMP_LOG_PERIOD = 5000;

struct primitive_feature {
	primitive_feature(string t, string n): type(t), name(n) {};// { array_dims = 0; };
	string type;
	string name;
};

struct dwarf_formal_parameter
{
	bool is_addr;
	bool is_reg_with_addr;
	bool is_ptr;
	uint64_t position;
	int64_t offset;
	string type_name;
	unordered_map<string, vector<primitive_feature>> runtime_type_to_features;
	uint32_t array_dims;
	vector<uint32_t> array_counts;
};

struct routine_descriptor 
{
	PIN_MUTEX history_mutex;
	unsigned int executed_count;	
	unsigned int taken_count;	
	string name;
	uint64_t address; // The address of the first instruction after the prologue
	uint16_t param_count;
	unsigned int max_features_count;

	vector<struct dwarf_formal_parameter> params;
	/*
	vector<bool> is_addr;
	vector<bool> is_reg_with_addr;
	vector<bool> is_ptr;
	vector<uint64_t> position;
	vector<int64_t> offset;
	vector<std::string> feature_type;
	vector<uint32_t> array_dims;
	vector<vector<uint32_t>> array_counts;
	vector<std::string> variable_names;

	// How many features a specific parameter (identified by the idx in the vector) has
	vector<unsigned int> feature_counts;
	
	// The codes denoting the feature types associated with the parameter 
	vector<std::string> feature_names;
	*/

	vector<struct rtn_execution *> history[2][MAXTHREADS];
	int first_execution;
	volatile bool active_history;

	// SYSTEM FEATURES
	vector<std::string> system_variable_names;

	void init() {
		PIN_MutexInit(&history_mutex);
		first_execution = 0;
		executed_count = taken_count = 0;
		active_history = false; // Not really needed
		for (int t = 0; t < MAXTHREADS; t++) {
			history[0][t].reserve(EXPECTED_SAMPLES_PER_PERIOD);
			history[1][t].reserve(EXPECTED_SAMPLES_PER_PERIOD);
		}
	}

	void acquire_lock() {
		PIN_MutexLock(&history_mutex);
	}

	void release_lock() {
		PIN_MutexUnlock(&history_mutex);
	}

	void add_to_history(struct rtn_execution * re, THREADID tid, int pos) {
		PIN_MutexLock(&history_mutex);
		// The second part may occur if log dump happened during the execution of the
		// instrumented symbol
		if (pos == -1 || (unsigned int)pos >= history[active_history][tid].size())
			history[active_history][tid].push_back(re);
		else {
			// evict the old sample
			delete history[active_history][tid][pos];
			history[active_history][tid][pos] = re;
		}
		PIN_MutexUnlock(&history_mutex);
	}

	void switch_history() {
		PIN_MutexLock(&history_mutex);
		taken_count = 0;
		active_history = !active_history;
		for (int t = 0; t < MAXTHREADS; t++) {
			for (struct rtn_execution * re: history[active_history][t]) {
				delete re;
			}
			history[active_history][t].clear();
		}
		PIN_MutexUnlock(&history_mutex);
	}

	inline size_t get_stored_history_size(const THREADID tid) const {
		return history[!active_history][tid].size();
	}
	
	inline const vector<struct rtn_execution *> * get_stored_history_ptr(const THREADID tid) const {
		return &(history[!active_history][tid]);
	}
	
	struct rtn_execution * get_sample(THREADID tid, const int idx) {
		if (history[active_history][tid].size() == 0) {
			// this must be the first execution of this symbol on this thread
			// so I'm not going to remove anything, even though I should
			// never mind, ignore this case
			return 0;
		}
		PIN_MutexLock(&history_mutex);
		struct rtn_execution * addr = history[active_history][tid][idx]; 	
		PIN_MutexUnlock(&history_mutex);
		return addr;
	}

};

#endif
