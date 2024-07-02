#ifndef RTN_EXECUTION_H_
#define RTN_EXECUTION_H_

#include <string>
#include <unordered_map>
#include <set>
#include <sys/time.h>

// TODO: these values depend on the processor, should be parsed automatically
#define TSC_CLOCK_RATE_SEC  2300000000u
#define TSC_CLOCK_RATE_MSEC 2300000u
#define TSC_CLOCK_RATE_USEC 2300u

//#define NO_CTX_SWITCH

using namespace std;

static __inline__ uint64_t rdtsc(){
#ifdef USE_OLD_RDTSC
    unsigned int lo,hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
#else
    // https://stackoverflow.com/questions/14783782/which-inline-assembly-code-is-correct-for-rdtscp
    unsigned long long tsc;
    __asm__ __volatile__(
        "rdtscp;"
        "shl $32, %%rdx;"
        "or %%rdx, %%rax"
        : "=a"(tsc)
        :
        : "%rcx", "%rdx");

    return tsc;
#endif
}

struct rtn_execution
{
	bool active;
	uint64_t start_time;
	uint64_t stop_time;
	uint64_t allocated_memory;
	uint64_t total_lock_holding_time;
	uint64_t total_waiting_time;
	uint64_t minor_page_faults;
	uint64_t major_page_faults;
	uint64_t voluntary_ctx_switches;
	uint64_t involuntary_ctx_switches;

	// SYSTEM FEATURES
	vector<int64_t> system_feature_values;

	bool started_with_lock;
	uint64_t descendants_instrumentation_time;
	uint32_t unique_id;
	std::vector<uint32_t> children;
	vector<int64_t> feature_values;
	vector<uint64_t> runtime_types;

	unordered_map<uint16_t, vector<bool>> branches;
	ADDRINT return_val;

	void init(uint32_t uid, bool lock) {
		total_waiting_time = descendants_instrumentation_time = start_time = stop_time = 0;
		active = true;
		// PROCFS stats
		// init to zero in case we are not reading procfs
		minor_page_faults = major_page_faults = voluntary_ctx_switches = involuntary_ctx_switches = 0;
		allocated_memory = 0;
		unique_id = uid;
		total_lock_holding_time = 0;
		started_with_lock = lock;
		// TODO: use meaningful values for these
		runtime_types.reserve(1);
		system_feature_values.reserve(1);
	}

	void trigger_start() {
		if (!active)
			return;
		start_time = rdtsc();
	}

	inline void add_runtime_type(uint64_t rt) {
		runtime_types.push_back(rt);
	}
	
	void add_feature_value(void ** arg) {
		// hoping to catch any non explicitly defined type here
		feature_values.push_back(0);
	}
	
	void add_feature_value(unsigned char ** arg) {
		feature_values.push_back(0);
	}

	template<typename T> 
	void add_feature_value(T ** arg) {
		// Skipping ptr2ptr!
		feature_values.push_back(0);
	}
	
	template<typename T> 
	void add_feature_value(T * arg) {
		T foo;
		if (arg && PIN_SafeCopy(&foo, arg, sizeof(T)) == sizeof(T))
			// looks like we can read this thing
			feature_values.push_back(foo);
		else 
			feature_values.push_back(0);
	}
	
	template<typename T> 
	void add_feature_value(T arg) {
		// looks like we can read this thing
		feature_values.push_back((int64_t)arg);
	}
	
	void add_feature_value(long long unsigned int* arg){
	 	long long unsigned int tmp;
		if (arg && PIN_SafeCopy(&tmp, arg, sizeof(long long unsigned int)) == sizeof(long long unsigned int)) {
			// looks like we can read this thing
			//if (foo > INT32_MAX)
			//	std::cout << "TODO: unsigned long long too big for uint32_t!" << std::endl; 
			//std::cout << "Added long long val! " << foo << std::endl; 
			feature_values.push_back(tmp % UINT32_MAX);
		}
		else 
			feature_values.push_back(0);
	}

	void add_feature_value(long unsigned int * arg){
		//std::cout << "Got value " << hex << (ADDRINT)arg << std::endl;
	 	long unsigned int foo;
		if (arg && PIN_SafeCopy(&foo, arg, sizeof(long unsigned int)) == sizeof(long unsigned int)) {
			// looks like we can read this thing
			//if (foo > INT32_MAX)
			//	std::cout << "TODO: unsigned long too big for uint32_t!" << std::endl; 
			//std::cout << "Added val! " << foo << std::endl; 
			feature_values.push_back(foo % UINT32_MAX);
		}
		else 
			feature_values.push_back(0);
		//std::cout << "Read 4 bytes " << arg << std::endl;
	}


#if UNSIGNED_CHAR_IS_STRING
	void add_feature_value(unsigned char * arg){
		// FIXME: does it make sense to consider this as a string?
		//std::cout << "Reading str from " << (uint64_t)arg << std::endl;
		unsigned char *tmp_ptr = arg;
		unsigned char foo;
		int len = 0;
		while (arg && PIN_SafeCopy(&foo, tmp_ptr++, 1) == 1 && foo != '\0')
			len++;
		feature_values.push_back(len);
	}
#endif

	void add_feature_value(char ** arg){
		// Try to compute the string length of the string pointed to by the first pointer (which is also the only one many time). And do it safely. This might be super slow
		char *tmp_ptr = 0;
		int len = 0;
		if (arg && PIN_SafeCopy(&tmp_ptr, arg, 1) == 1) {
			char foo;
			while (tmp_ptr && PIN_SafeCopy(&foo, tmp_ptr++, 1) == 1 && foo != '\0')
				len++;
		}
		feature_values.push_back(len);
	}

	void add_feature_value(char * arg){
		// Compute the (maybe not null terminated) string length, and do it safely. This might be super slow
		char *tmp_ptr = arg;
		char foo;
		int len = 0;
		while (arg && PIN_SafeCopy(&foo, tmp_ptr++, 1) == 1 && foo != '\0')
			len++;
		feature_values.push_back(len);
	}


#include <stdarg.h>

	// ARRAYS
	template<typename T>
	void add_feature_value_array(T * arg, const unsigned int dims, const vector<uint32_t> * counts) {
		void * foo;
		uint32_t res = 1;
		T sum = 0;
		if (arg && PIN_SafeCopy(&foo, arg, sizeof(void *)) == sizeof(void *)) {
			// the first feature is the total length of the array
			for (uint32_t c : *counts) {
				//printf("Adding dim %u / %u\n", c, dims); 
				res *= (c + 1u);
			}
			feature_values.push_back((int)res);
			// An array is guaranteed to have contiguous memory allocation
			// so this should not break...
			for (uint32_t i = 0; i < res; i++) {
				sum += arg[i];
			} 
			feature_values.push_back((int)sum);
		}
		else {
			feature_values.push_back(0);
			feature_values.push_back(0);
		}
	}

	template<typename T>
	void add_feature_value_array_var(T * arg, const unsigned int dims, ...) {
		void * foo;
		uint32_t res = 1;
		T sum = 0;
		if (arg && PIN_SafeCopy(&foo, arg, sizeof(void *)) == sizeof(void *)) {
			va_list ap;
			va_start(ap, dims);
			// the first feature is the total length of the array
			for (uint32_t c = 0; c < dims; c++)
				res *= (va_arg(ap, uint32_t) + 1u);
			va_end(ap);
			feature_values.push_back((int)res);
			// An array is guaranteed to have contiguous memory allocation
			// so this should not break...
			for (uint32_t i = 0; i < res; i++) {
				//printf("Reading array, adding %u, value %d\n", i, (int)arg[i]);
				sum += arg[i];
			} 
			feature_values.push_back((int)sum);
		}
		else {
			feature_values.push_back(0);
			feature_values.push_back(0);
		}
	}

	void do_malloc(uint64_t mem_size) {
		allocated_memory += mem_size;
	}

	void released_all_locks(const uint64_t tot_time, const uint64_t release_time) {
		if (!started_with_lock)
			total_lock_holding_time += tot_time;
		else 
			total_lock_holding_time += release_time - start_time;
		
		// Should this function grab another lock, that must be considered from
		// the lock grabbing time 
		started_with_lock = false;
	}
	
	void done_waiting(const uint64_t tot_time) {
		total_waiting_time += tot_time;
	}

	void init_stats(uint64_t minor, uint64_t major, uint64_t vcsw, uint64_t ivcsw) {
		minor_page_faults = minor;
		major_page_faults = major;
		voluntary_ctx_switches = vcsw;
		involuntary_ctx_switches = ivcsw;
	}
	
	void trigger_end(uint64_t itime, uint64_t locks, uint64_t lock_start, uint64_t minor_pf, uint64_t major_pf, uint64_t vctxs, uint64_t ivctxs){
		if(!active)
			return;
		
		stop_time = rdtsc();

		if (locks > 0 && started_with_lock == false) {
			// this guy acquired a lock himself!
			total_lock_holding_time	+= (stop_time - lock_start) - descendants_instrumentation_time;
		}
		minor_page_faults = minor_pf - minor_page_faults;
		major_page_faults = major_pf - major_page_faults;
		voluntary_ctx_switches = vctxs - voluntary_ctx_switches;
		involuntary_ctx_switches = ivctxs - involuntary_ctx_switches;
		
#ifdef NO_CTX_SWITCH
		if (voluntary_ctx_switches | involuntary_ctx_switches) {
			std::cout << minor_page_faults << " | " << major_page_faults << " | " << voluntary_ctx_switches << " | " << involuntary_ctx_switches << std::endl;
			
			// TODO: handle properly, instead of simply thrashing
			return;
		}
#endif
		descendants_instrumentation_time = itime;
		active = false;
	}

	uint64_t diff(){
		if(active)
			return UINT64_MAX;
		uint64_t duration = stop_time - start_time;
		if (started_with_lock) {
			// if we're here, this function never saw a lock release, but it's
			// kept the lock for the whole duration
			total_lock_holding_time = duration - descendants_instrumentation_time;
		} 
		total_lock_holding_time /= TSC_CLOCK_RATE_USEC;
		total_waiting_time /= TSC_CLOCK_RATE_USEC;
		return (duration - descendants_instrumentation_time) / TSC_CLOCK_RATE_USEC;
	}

};

#endif
