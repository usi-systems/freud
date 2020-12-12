#include <fstream>
#include <iostream>
#include <string>
#include <set>
#include <unordered_map>
#include <stdint.h>
#include "pin.H"
#include <cxxabi.h>

#include "logger.cc"
#include "rtn_descriptor.h"
#include "reader.hh"
#include "reader.cc" // TODO: fix makefiles

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>


/*******
* KNOBS, a.k.a. command line arguments
********/
KNOB<bool> KnobUseProcFS(KNOB_MODE_WRITEONCE, "pintool", "procfs", "true", "Read from procfs (needed for cpu speed)");
KNOB<bool> KnobFindBranches(KNOB_MODE_WRITEONCE, "pintool", "branches", "true", "Find branches");
KNOB<bool> KnobMeasureMemory(KNOB_MODE_WRITEONCE, "pintool", "memory", "false", "Measure memory allocations");
KNOB<bool> KnobMeasureLocks(KNOB_MODE_WRITEONCE, "pintool", "locks", "false", "Measure locks related metrics");
KNOB<bool> KnobMeasurePageFaults(KNOB_MODE_WRITEONCE, "pintool", "pfaults", "false", "Measure page faults (needs procfs)");
KNOB<unsigned int> KnobDumpPeriod(KNOB_MODE_WRITEONCE, "pintool", "dump_period", "5000", "Time (ms) between consecutive flush to the output");
KNOB<unsigned int> KnobLogsCount(KNOB_MODE_WRITEONCE, "pintool", "logs_count", "1000", "How many samples to take per dump_period for each symbol");
KNOB<unsigned int> KnobVerbosityLevel(KNOB_MODE_WRITEONCE, "pintool", "verbosity_level", "1", "Verbosity level");

#define GET_PROCFS_INFO
//#define DEBUG // should be disabled by default to avoid unnecessary overhead

struct thread_status {
	void init() {
		unique_id = current_locks_held = lock_start = cond_wait_start = mutex_wait_start = 0;
	};
	uint32_t unique_id;
	uint64_t current_locks_held;
	uint64_t lock_start;
	uint64_t cond_wait_start;
	uint64_t mutex_wait_start;
	vector<struct activation_record> activation_stack;
};

struct thr_file_handlers {
	std::ifstream * stat_file;
	std::ifstream * status_file;
};

struct cpu_file_handlers {
	std::ifstream * clock_file;
};

/*******
* GLOBALS
********/
struct thread_status tstatus[MAXTHREADS];

// DUMP LOG THEAD
bool quit_dump_thread;
PIN_THREAD_UID dlt_uid;

struct activation_record {
	std::string rtn_name;
	uint64_t instrumentation_time;
	rtn_execution * ptr;
};

unordered_map<std::string, std::set<uint64_t>> unique_branches;
unordered_map<OS_THREAD_ID, struct thr_file_handlers> tfile_handlers;
unordered_map<unsigned int, struct cpu_file_handlers> cfile_handlers;
unordered_map<std::string, struct routine_descriptor> routines_catalog;

static OS_PROCESS_ID pid;
uint64_t base_address;
unsigned int tot_found_with_reloc = 0;

// FWD DECL
inline unsigned long freud_hash(const unsigned char *str);
#include "dumper.cc"

// This function is called when the application exits
// It prints the name and count for each procedure
VOID Fini(INT32 code, VOID *v)
{
	log(VL_INFO, "Terminating the program, waiting for the last dump");
	quit_dump_thread = true;
	if (PIN_WaitForThreadTermination(dlt_uid, KnobDumpPeriod.Value() + 5000, NULL)) {
		// JOIN
		log(VL_DEBUG, "Dumping thread terminated correctly");
	} else {
		log(VL_ERROR, "Dumping thread was not terminating!");
	}
}
/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 usage()
{
	cerr << "This Pintool instruments the program in a very hackish way" << endl;
	cerr << "and it's probably not gonna work as you expected. Enjoy!" << endl;
	cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
	return -1;
}

#ifdef GET_PROCFS_INFO
std::ifstream * open_stat_fhandler(OS_THREAD_ID otid) {
	log(VL_DEBUG, "Opening stat for new thread");
	std::stringstream ss;
	ss << "/proc/";
	ss << pid;
	ss << "/task/";
	ss << otid;
	ss << "/stat";
	std::string maps_fname = ss.str();
	std::ifstream * stats = new std::ifstream(maps_fname.c_str());
	if (!stats->is_open())
		log(VL_ERROR, "Could not open " + maps_fname);
	tfile_handlers[otid].stat_file = stats;
	return stats;	
}

std::ifstream * open_status_fhandler(OS_THREAD_ID otid) {
	log(VL_DEBUG, "Opening status for new thread");
	std::stringstream ss;
	ss << "/proc/";
	ss << pid;
	ss << "/task/";
	ss << otid;
	ss << "/status";
	std::string maps_fname = ss.str();
	std::ifstream * stats = new std::ifstream(maps_fname.c_str());
	if (!stats->is_open())
		log(VL_ERROR, "Could not open " + maps_fname);
	tfile_handlers[otid].status_file = stats;
	return stats;	
}

// TODO: class all procfs file handlers!
void open_freq_handlers() {
	std::stringstream ss;
	unsigned int c = 0;
	while (true) {
		ss.str("/sys/devices/system/cpu/cpu");
		ss << c;
		ss << "/cpufreq/scaling_cur_freq";	
		std::ifstream * cspeed = new std::ifstream(ss.str().c_str());
		if (cspeed->is_open())
			cfile_handlers[c++].clock_file = cspeed;
		else
			break;
	}
	ss.str("Found ");
	ss << c;
	ss << " cores";
	log(VL_DEBUG, ss.str());
}

// For the offsets for different values look here:
// https://www.kernel.org/doc/Documentation/filesystems/proc.txt
uint64_t inline get_cpu_speed(std::ifstream * stat_handler) {
	uint64_t speed = 0;
	std::stringstream ss;
	uint16_t core;
	// Find which core is executing the thread
	stat_handler->seekg(0, ios::beg);
	for (int i = 0; i < 38; i++) {
		stat_handler->ignore(numeric_limits<streamsize>::max(), ' ');
	}
	(*stat_handler) >> core;

	// Read the clock freq for such core
	std::ifstream * cspeed = cfile_handlers[core].clock_file;
	cspeed->seekg(0, ios::beg);
	(*cspeed) >> speed;
	return speed;
}

/* 
 * Get thread statistics (page faults, ctx switches)
 * We cannot use rusage, since that is in libc, but not in Pin's libc
 * getrusage( RUSAGE_THREAD, &usage);
 *
 * So, we have to parse the proc filesystem.
 * /proc/PID/task/TID/stat contains page_faults information, and the last
 * 	cpu core used by the thread
 *
 * /proc/PID/task/TID/status contains context_switches information
 *
 * The documentation for these files is at "man proc", or 
 * http://man7.org/linux/man-pages/man5/proc.5.html
 */
VOID inline get_pfaults_cswitches(std::ifstream * stat_handler, uint64_t &mPf, uint64_t &MPf, std::ifstream * status_handler, uint64_t &volsw, uint64_t &invsw) {
	// MINOR PAGE FAULTS
	stat_handler->seekg(0, ios::beg);
	for (int i = 0; i < 9; i++)
		stat_handler->ignore(numeric_limits<streamsize>::max(), ' ');
	(*stat_handler) >> mPf;
		
	// MAJOR PAGE FAULTS
	for (int i = 0; i < 2; i++)
		stat_handler->ignore(numeric_limits<streamsize>::max(), ' ');
	(*stat_handler) >> MPf;
	
	// CTX SWITCHES
	status_handler->seekg(0, ios::beg);
	for (int i = 0; i < 53; i++)
		status_handler->ignore(numeric_limits<streamsize>::max(), '\n');
	
	std::string check;
	(*status_handler) >> check;
	(*status_handler) >> volsw;
	(*status_handler) >> check;
	(*status_handler) >> invsw;

	if (check != "nonvoluntary_ctxt_switches:")
		log(VL_ERROR, "Error parsing context switches!");
}
#endif

inline unsigned long freud_hash(const unsigned char *str) {                      	 
	unsigned long hash = 5381;
	int c;
	while ((c = *str++))
		hash = ((hash << 5) + hash) + c;
	return hash;
}


VOID start_of_execution(struct routine_descriptor *desc,
			THREADID tid,
			CONTEXT * ctx
){
#ifdef DEBUG
	// No std::to_string() in STL...
	std::ostringstream oss;
	oss << "Executing " << desc->name << " on tid " << tid << " (" << desc->taken_count << " execution)";
	log(VL_DEBUG, oss.str());
#endif
	if (tid >= MAXTHREADS) {
		log(VL_ERROR, "Not enough threads, dropping something");
		return;
	}
	
	// TAKE INSTRUMENTATION START POINT
	uint64_t instr_start_time;
	instr_start_time = rdtsc();
	desc->executed_count += 1;
	int sample_position = -1;

	if (desc->taken_count < KnobLogsCount.Value()) {
		// filling the reservoir array
		// take it, with probability 1
	} else {
		// Reservoir sampling kicks in
		unsigned int r = rand() % desc->executed_count;
		if (r < KnobLogsCount.Value()) {
			// take the new
			sample_position = r;
			struct rtn_execution * tbr = desc->get_sample(tid, r);
			unsigned int i = 0;
			for (; i < tstatus[tid].activation_stack.size(); i++) {
				if (tstatus[tid].activation_stack[i].ptr == tbr) {
					tstatus[tid].activation_stack[i].ptr = 0;
					break;
				}
			}
		}
		else {
			// ignore this sample
			// clean up and return
			struct activation_record ar;
			ar.rtn_name = desc->name;
			ar.ptr = NULL;
			ar.instrumentation_time = 0;
			tstatus[tid].activation_stack.push_back(ar);
			
			uint64_t instr_stop_time = rdtsc();
			uint64_t duration = instr_stop_time - instr_start_time;
			for (struct activation_record & ar_s: tstatus[tid].activation_stack)
				ar_s.instrumentation_time += duration;
			
			return;
		}
	}

	// Integer register
	ADDRINT input_args[8] = {
		PIN_GetContextReg(ctx, REG_RDI),
		PIN_GetContextReg(ctx, REG_RSI),
		PIN_GetContextReg(ctx, REG_RDX),
		PIN_GetContextReg(ctx, REG_RCX),
		PIN_GetContextReg(ctx, REG_R8),
		PIN_GetContextReg(ctx, REG_R9),
		PIN_GetContextReg(ctx, REG_RSP),
		PIN_GetContextReg(ctx, REG_RBP),
	};

	// FP registers
	/*
	FLT64 finput_args[7];
	FIXME: XMM registers are 128byte long, they do not fit in FLT64...
	  
	PIN_GetContextRegval(ctx, REG_XMM0, (UINT8 *)(finput_args+0));
	PIN_GetContextRegval(ctx, REG_XMM1, (UINT8 *)(finput_args+1));
	PIN_GetContextRegval(ctx, REG_XMM2, (UINT8 *)(finput_args+2));
	PIN_GetContextRegval(ctx, REG_XMM3, (UINT8 *)(finput_args+3));
	PIN_GetContextRegval(ctx, REG_XMM4, (UINT8 *)(finput_args+4));
	PIN_GetContextRegval(ctx, REG_XMM5, (UINT8 *)(finput_args+5));
	PIN_GetContextRegval(ctx, REG_XMM6, (UINT8 *)(finput_args+6));
	*/

	// There is no thread-safety on taken_count
	// This means that we may collect slightly more samples than EXPECTED
	// But it doesn't hurt, and we do not want to sync threads on this
	desc->taken_count++;

	// Generate uid for this sample
	struct rtn_execution * re = new struct rtn_execution;
	uint32_t size_for_thread = (1 << 26);
	if (tstatus[tid].unique_id >= size_for_thread) {
#ifndef DEBUG
		std::ostringstream oss;
#endif
		oss.str("Running out of uid on tid ");
		oss << tid;
		log(VL_ERROR, oss.str());
		exit(-1);
	}
	uint32_t my_uid = size_for_thread * (uint32_t)tid + tstatus[tid].unique_id++;

	// Add ourselves as a son of the top of the stack (the last rtn called) 
	if (tstatus[tid].activation_stack.size() && tstatus[tid].activation_stack.back().ptr)
		tstatus[tid].activation_stack.back().ptr->children.push_back(my_uid);

	// Init the sample descriptor
	re->init(my_uid, tstatus[tid].current_locks_held > 0);

	// Collect the values of the features
	for (unsigned int q = 0; q < desc->param_count; q++) {
#ifdef DEBUG
		if (desc->params[q].runtime_type_to_features.size() == 1) {
			oss.str("Processing (");
		} else {
			oss.str("Processing w/ dyn. polymorphism (");
		}
		oss << q << "/" << desc->param_count << ") ";
		oss << re->feature_values.size();
		log(VL_DEBUG, oss.str());
#endif

		// This is the actual body of the loop
		// The code is generated by the dwarf phase
		#include "feature_processing.cc"

#ifdef DEBUG
		log(VL_DEBUG, "Done with parameter");
#endif
	}

	// Insert a pointer to the sample in our collection
	desc->add_to_history(re, tid, sample_position);

	struct activation_record ar;
	ar.rtn_name = desc->name;
	ar.ptr = re;
	ar.instrumentation_time = 0;

#ifdef GET_PROCFS_INFO
	if (KnobUseProcFS.Value()) {
		// Collect info from procfs
		OS_THREAD_ID otid = PIN_GetTid();
		std::ifstream * stat_handler = 0, * status_handler = 0;
		std::unordered_map<OS_PROCESS_ID, struct thr_file_handlers>::const_iterator h_files = tfile_handlers.find(otid);
		if (h_files == tfile_handlers.end()) {
#ifdef DEBUG
			oss.str("Opening proc stats for tid ");
			oss << otid;
			log(VL_DEBUG, oss.str());
#endif
			stat_handler = open_stat_fhandler(otid);
			status_handler = open_status_fhandler(otid);
			struct thr_file_handlers & th = tfile_handlers[otid];
			th.stat_file = stat_handler;
			th.status_file = status_handler;
		} else {
			stat_handler = h_files->second.stat_file;
			status_handler = h_files->second.status_file;
		}
		if (KnobMeasurePageFaults.Value()) {
			get_pfaults_cswitches(stat_handler, re->minor_page_faults, re->major_page_faults, status_handler, re->voluntary_ctx_switches, re->involuntary_ctx_switches);
		}
	
		// CPU CLOCK	
		re->system_feature_values.push_back(get_cpu_speed(stat_handler));
	}
#endif

	// Instrumentation has finished... almost. Take a timestamp
	uint64_t instr_stop_time = rdtsc();
	uint64_t duration = instr_stop_time - instr_start_time;

	// TODO: move this loop somewhere where it won't be considered in the symbol execution time
	for (struct activation_record & ar_s: tstatus[tid].activation_stack)
		ar_s.instrumentation_time += duration;
	
	tstatus[tid].activation_stack.push_back(ar);

	// Take a timestamp for the beginning of execution
	// Finally, start the execution of the actual function
	re->trigger_start();
}

VOID memory_call(ADDRINT memory_size, THREADID tid)
{
#ifdef DEBUG
	std::ostringstream oss;
	oss.str("tid ");
	oss << tid << " allocated memory";
	log(VL_DEBUG, oss.str());
#endif
	if (tid < MAXTHREADS) {
		for (struct activation_record & ar_s: tstatus[tid].activation_stack) {
			if (ar_s.ptr) {
				ar_s.ptr->do_malloc(memory_size);
			}
		}
	}
}

VOID lock_acquired_by_cond(ADDRINT result, THREADID tid, uint64_t time)
{
	uint64_t lock_start_time = time;
	// If the call succeeded...
	if (result == 0) {
#ifdef DEBUG
		std::ostringstream oss;
		oss.str("tid ");
		oss << tid << " acquired a lock on condition variable";
		log(VL_DEBUG, oss.str());
#endif
		if (tstatus[tid].current_locks_held == 0) {
			// It's the first lock, set the timer
			tstatus[tid].lock_start = lock_start_time;
		}
		tstatus[tid].current_locks_held++;
	}
	// No need to consider the waiting time here...
}

VOID lock_acquired(ADDRINT result, THREADID tid)
{
	uint64_t lock_start_time = rdtsc();
	// If the call succeeded...
	if (result == 0) {
#ifdef DEBUG
		std::ostringstream oss;
		oss.str("tid ");
		oss << tid << " acquired a lock";
		log(VL_DEBUG, oss.str());
#endif
		if (tstatus[tid].current_locks_held == 0) {
			// It's the first lock, set the timer
			tstatus[tid].lock_start = lock_start_time;
		}
		tstatus[tid].current_locks_held++;
	}
	if (tstatus[tid].mutex_wait_start == 0) {
		log(VL_ERROR, "Mutex Wait start was zero, this is a problem");
		exit(-1);
	}

	uint64_t total_wait = lock_start_time - tstatus[tid].mutex_wait_start;
	for (struct activation_record & ar_s: tstatus[tid].activation_stack) {
		if (ar_s.ptr) {
			ar_s.ptr->done_waiting(total_wait);
		}
	}
	tstatus[tid].mutex_wait_start = 0;
}

VOID lock_released_by_cond(ADDRINT result, THREADID tid, uint64_t time)
{
	uint64_t lock_stop = time;
	// If the call succeeded...
	if (result == 0) {
		tstatus[tid].current_locks_held--;
#ifdef DEBUG
		std::ostringstream oss;
		oss.str("tid ");
		oss << tid << " released lock";
		log(VL_DEBUG, oss.str());
#endif
		if (tstatus[tid].current_locks_held == 0) {
			// Not holding any more locks
			uint64_t lock_time = lock_stop - tstatus[tid].lock_start;
			for (struct activation_record & ar_s: tstatus[tid].activation_stack) {
				if (ar_s.ptr) {
					ar_s.ptr->released_all_locks(lock_time, lock_stop);
				}
			}
		}
	}
}

VOID lock_released(ADDRINT result, THREADID tid)
{
	uint64_t lock_stop = rdtsc();
	// If the call succeeded...
	if (result == 0) {
		tstatus[tid].current_locks_held--;
#ifdef DEBUG
		std::ostringstream oss;
		oss.str("tid ");
		oss << tid << " released lock";
		log(VL_DEBUG, oss.str());
#endif
		if (tstatus[tid].current_locks_held == 0) {
			// Not holding any more locks
			uint64_t lock_time = lock_stop - tstatus[tid].lock_start;
			for (struct activation_record & ar_s: tstatus[tid].activation_stack) {
				if (ar_s.ptr) {
					ar_s.ptr->released_all_locks(lock_time, lock_stop);
				}
			}
		}
	}
}

VOID mutex_acquire_called(THREADID tid)
{
	uint64_t mutex_wait_start_time = rdtsc();
#ifdef DEBUG
	std::ostringstream oss;
	oss.str("tid ");
	oss << tid << " started waiting";
	log(VL_DEBUG, oss.str());
#endif
	if (tstatus[tid].mutex_wait_start != 0) {
		log(VL_ERROR, "Mutex wait start was set already; Pin missed the exit point from pthread_mutex_lock()?");
		//exit(-1);
	}
	tstatus[tid].mutex_wait_start = mutex_wait_start_time;
}

VOID condwait_called(THREADID tid)
{
	uint64_t cond_wait_start_time = rdtsc();
#ifdef DEBUG
	std::ostringstream oss;
	oss.str("tid ");
	oss << tid << " started waiting";
	log(VL_DEBUG, oss.str());
#endif
	if (tstatus[tid].cond_wait_start != 0) {
		log(VL_ERROR, "Cond wait start was set already; Pin missed the exit point from pthread_cond_wait()?");
		//exit(-1);
	}
	tstatus[tid].cond_wait_start = cond_wait_start_time;
	// This implicitly releases the lock!
	lock_released_by_cond(0, tid, cond_wait_start_time);
}

VOID condwait_returned(THREADID tid)
{
	// I'm not interested in the result; even if the call failed, we still spent time in the waiting function...

	uint64_t wait_stop_time = rdtsc();
#ifdef DEBUG
	std::ostringstream oss;
	oss.str("tid ");
	oss << tid << " stopped waiting";
	log(VL_DEBUG, oss.str());
#endif
	uint64_t total_wait = wait_stop_time - tstatus[tid].cond_wait_start;
	if (tstatus[tid].cond_wait_start == 0) {
		log(VL_ERROR, "Wait start was zero, this is a problem");
		exit(-1);
	}
	for (struct activation_record & ar_s: tstatus[tid].activation_stack) {
		if (ar_s.ptr) {
			ar_s.ptr->done_waiting(total_wait);
		}
	}
	tstatus[tid].cond_wait_start = 0;
	// This implicitly releases the lock!
	lock_acquired_by_cond(0, tid, wait_stop_time);
}

VOID locking_routines(IMG img, VOID *v)
{
    // Instrument all the functions that should lock a shared resource.  
    RTN lockRtn = RTN_FindByName(img, "pthread_mutex_lock");
    if (RTN_Valid(lockRtn))
    {
	log(VL_DEBUG, "Lock found!");
        RTN_Open(lockRtn);
        RTN_InsertCall(lockRtn, IPOINT_BEFORE, (AFUNPTR)mutex_acquire_called, IARG_THREAD_ID, IARG_END);
        RTN_InsertCall(lockRtn, IPOINT_AFTER, (AFUNPTR)lock_acquired, IARG_FUNCRET_EXITPOINT_VALUE, IARG_THREAD_ID, IARG_END);
        RTN_Close(lockRtn);
    }

    lockRtn = RTN_FindByName(img, "pthread_mutex_trylock");
    if (RTN_Valid(lockRtn))
    {
	log(VL_DEBUG, "TryLock found!");
        RTN_Open(lockRtn);
        RTN_InsertCall(lockRtn, IPOINT_AFTER, (AFUNPTR)lock_acquired, IARG_FUNCRET_EXITPOINT_VALUE, IARG_THREAD_ID, IARG_END);
        RTN_Close(lockRtn);
    }

    RTN unlockRtn = RTN_FindByName(img, "pthread_mutex_unlock");
    if (RTN_Valid(unlockRtn))
    {
	log(VL_DEBUG, "UnLock found!");
        RTN_Open(unlockRtn);

        // Instrument malloc() to print the input argument value and the return value.
        RTN_InsertCall(unlockRtn, IPOINT_AFTER, (AFUNPTR)lock_released, IARG_FUNCRET_EXITPOINT_VALUE, IARG_THREAD_ID, IARG_END);

        RTN_Close(unlockRtn);
    }

    RTN cwRtn = RTN_FindByName(img, "pthread_cond_wait");
    if (RTN_Valid(cwRtn))
    {
	log(VL_DEBUG, "CondWait found!");
        RTN_Open(cwRtn);

        RTN_InsertCall(cwRtn, IPOINT_BEFORE, (AFUNPTR)condwait_called, IARG_THREAD_ID, IARG_END);
        RTN_InsertCall(cwRtn, IPOINT_AFTER, (AFUNPTR)condwait_returned, IARG_THREAD_ID, IARG_END);

        RTN_Close(cwRtn);
    }
    RTN ctwRtn = RTN_FindByName(img, "pthread_cond_timedwait");
    if (RTN_Valid(ctwRtn))
    {
	log(VL_DEBUG, "CondTimedWait found!");
        RTN_Open(ctwRtn);

        RTN_InsertCall(ctwRtn, IPOINT_BEFORE, (AFUNPTR)condwait_called, IARG_THREAD_ID, IARG_END);
        RTN_InsertCall(ctwRtn, IPOINT_AFTER, (AFUNPTR)condwait_returned, IARG_THREAD_ID, IARG_END);

        RTN_Close(ctwRtn);
    }
}

VOID malloc_routine(IMG img, VOID *v)
{
    // Instrument the malloc() and free() functions.  Print the input argument
    // of each malloc() or free(), and the return value of malloc().
    //
    //  Find the malloc() function.
    RTN mallocRtn = RTN_FindByName(img, "malloc");
    if (RTN_Valid(mallocRtn))
    {
        RTN_Open(mallocRtn);

        // Instrument malloc() to print the input argument value and the return value.
        RTN_InsertCall(mallocRtn, IPOINT_BEFORE, (AFUNPTR)memory_call, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_THREAD_ID, IARG_END);

        RTN_Close(mallocRtn);
    }
}

VOID end_of_execution(struct routine_descriptor *desc, THREADID tid) {
#ifdef DEBUG
	std::ostringstream oss;
	oss.str("Exiting ");
	oss << desc->name << " on thread" << tid;
	log(VL_DEBUG, oss.str());
#endif
	
	// The "trigger_end" method takes some time, so I want to account for that
	uint64_t instr_start_time = rdtsc();
	
	if (tid >= MAXTHREADS) return;
	
	uint64_t min_pf = 0, maj_pf = 0, vol_cs = 0, inv_cs = 0;

#ifdef GET_PROCFS_INFO
	OS_THREAD_ID otid = PIN_GetTid();
	std::unordered_map<OS_PROCESS_ID, struct thr_file_handlers>::const_iterator thandlers = tfile_handlers.find(otid);
	// If the all the previous executions by this thread were sampled out, than this handler is never opened
	if (KnobMeasurePageFaults.Value() && (thandlers != tfile_handlers.end()))
		get_pfaults_cswitches(thandlers->second.stat_file, min_pf, maj_pf, thandlers->second.status_file, vol_cs, inv_cs);
#endif	

	if (!tstatus[tid].activation_stack.empty()) {
		// It can happen that this code is not triggered by the top of the stack function
		// According to Pin manual, we can miss some exit points at times
		// So, remove from the stack all the "unexited" functions first, assuming that they
		// terminated at the same time of the currently exiting function
		while (tstatus[tid].activation_stack.back().rtn_name != desc->name) {
#ifdef DEBUG
			log(VL_DEBUG, "Removing leftovers from the stack!");
#endif
			if (tstatus[tid].activation_stack.back().ptr) {
				uint64_t duration = rdtsc() - instr_start_time;
				tstatus[tid].activation_stack.back().ptr->trigger_end(tstatus[tid].activation_stack.back().instrumentation_time + duration, tstatus[tid].current_locks_held, tstatus[tid].lock_start, min_pf, maj_pf, vol_cs, inv_cs);
			}
			tstatus[tid].activation_stack.pop_back();
			if (tstatus[tid].activation_stack.empty())
				break;
		}
		if (!tstatus[tid].activation_stack.empty() && tstatus[tid].activation_stack.back().rtn_name == desc->name) {
			if (tstatus[tid].activation_stack.back().ptr) {
				uint64_t duration = rdtsc() - instr_start_time;
				tstatus[tid].activation_stack.back().ptr->trigger_end(tstatus[tid].activation_stack.back().instrumentation_time + duration, tstatus[tid].current_locks_held, tstatus[tid].lock_start, min_pf, maj_pf, vol_cs, inv_cs);
			}
			tstatus[tid].activation_stack.pop_back();
			if (tstatus[tid].activation_stack.size() > 50) {
				tstatus[tid].activation_stack.clear();
			}
		}

		// If there's something else being executed, 
		// add the time spent in this function
		uint64_t duration = rdtsc() - instr_start_time;
		for (struct activation_record & ar_s: tstatus[tid].activation_stack)
			ar_s.instrumentation_time += duration;
	}
}

static VOID is_branch_taken(UINT32 a, BOOL taken, THREADID tid) {
	if (tid < MAXTHREADS) {
		if (tstatus[tid].activation_stack.size() && tstatus[tid].activation_stack.back().ptr)
			tstatus[tid].activation_stack.back().ptr->branches[a].push_back(taken);
	}
}

VOID instr_branches(INS ins, VOID *v) {
	string fname = RTN_FindNameByAddress(INS_Address(ins));
	if (routines_catalog.find(fname) == routines_catalog.end())
		return;
	
	/* I need to compare the full addr of the INS because:
	 * "certain BBLs and the instructions inside of them may be generated (and hence instrumented) multiple times"
	 * source: http://people.cs.vt.edu/~gback/cs6304.spring07/pin2/Doc/Pin/html/index.html
	 */
	ADDRINT iaddr = INS_Address(ins) - base_address;
	uint16_t b_id = iaddr;
	bool first_round = true;

check_uniqueness:
	for (uint64_t prev_b: unique_branches[fname]) {
		uint16_t prev_b_id = prev_b;
		if (prev_b != iaddr && prev_b_id == b_id) {
			if (!first_round) {
				std::ostringstream oss("Found a non-unique branch for ");
				oss << fname << ", this is a problem. " << b_id << " - " << iaddr;
				log(VL_ERROR, oss.str());
				oss.str("");
				oss << prev_b << " - " << prev_b_id;
				log(VL_ERROR, oss.str());
				exit(-1);
			}
			long diff = prev_b - iaddr;

			b_id += abs(diff) / 65536; // the modulo is the same, so compute the dist
			first_round = false;
			goto check_uniqueness;
		}
	}
	unique_branches[fname].insert(iaddr);

	if (INS_IsBranch(ins) && INS_HasFallThrough(ins)) {
		// this should be an if
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)is_branch_taken, IARG_UINT32, b_id, IARG_BRANCH_TAKEN, IARG_THREAD_ID, IARG_END);
	}
}

VOID instrument_function(RTN rtn, VOID *v) {
	if (routines_catalog.find(RTN_Name(rtn)) == routines_catalog.end())
		return;
	log(VL_DEBUG, "Found routine " + RTN_Name(rtn));
	routines_catalog[RTN_Name(rtn)].init();

	// We take all the registers by default
	// TODO: take only the registers that are really needed.
	// The relevant info us in routines_catalog
	REGSET regsIn;
	REGSET_Clear(regsIn);
	REGSET_Insert(regsIn, REG_RDI);
	REGSET_Insert(regsIn, REG_RSI);
	REGSET_Insert(regsIn, REG_RDX);
	REGSET_Insert(regsIn, REG_RCX);
	REGSET_Insert(regsIn, REG_R8);
	REGSET_Insert(regsIn, REG_R9);
	REGSET_Insert(regsIn, REG_RSP);
	REGSET_Insert(regsIn, REG_RBP);
	// floating point registers
	REGSET_Insert(regsIn, REG_XMM0);
	REGSET_Insert(regsIn, REG_XMM1);
	REGSET_Insert(regsIn, REG_XMM2);
	REGSET_Insert(regsIn, REG_XMM3);
	REGSET_Insert(regsIn, REG_XMM4);
	REGSET_Insert(regsIn, REG_XMM5);
	REGSET_Insert(regsIn, REG_XMM6);
	REGSET regsOut;
	REGSET_Clear(regsOut);

	RTN_Open(rtn);
	bool found = false;

	// Insert a call at the first instruction **after** the prologue of a routine 
	for( INS ins= RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins) ) {
		if (INS_Address(ins) - base_address == routines_catalog[RTN_Name(rtn)].address) {
			log(VL_DEBUG, "Found entry point!");
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)start_of_execution, 
				IARG_UINT64, &(routines_catalog.find(RTN_Name(rtn))->second),
				IARG_THREAD_ID,
				IARG_PARTIAL_CONTEXT,
				&regsIn,
				&regsOut,
				IARG_END);
			tot_found_with_reloc++;
			found = true;
			break;
		}

		/*
		* Check whether the base address should not be considered.
		* This might happen for non Position Independent Code
		* This is hacky, but I'm not sure how to handle this properly
		*
		*/
		if (INS_Address(ins) == routines_catalog[RTN_Name(rtn)].address) {
			log(VL_DEBUG, "Found entry point at absolute address!");
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)start_of_execution, 
				IARG_UINT64, &(routines_catalog.find(RTN_Name(rtn))->second),
				IARG_THREAD_ID,
				IARG_PARTIAL_CONTEXT,
				&regsIn,
				&regsOut,
				IARG_END);
			found = true;

			if (tot_found_with_reloc == 0 && base_address != 0) {
				// base_address should probably be set to 0
				// Like above, this is hacky; should find a proper solution
				log(VL_ERROR, "Setting base_address to 0!");
				log(VL_ERROR, "Code compiled without -fPIC?");
				base_address = 0; 
			}
			break;
		}
	}

	if (!found) {
		log(VL_ERROR, "Cannot find entry point for " + RTN_Name(rtn) + "; trying before rtn.");
		log(VL_ERROR, "### THIS MIGHT CAUSE UNDEFINED BEHAVIOR! ###");
		log(VL_ERROR, "Maybe the target binary has been recompiled after the execution of freud-dwarf?");
		RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)start_of_execution, 
			IARG_UINT64, &(routines_catalog.find(RTN_Name(rtn))->second),
			IARG_THREAD_ID,
			IARG_PARTIAL_CONTEXT,
			&regsIn,
			&regsOut,
			IARG_END);	
	}

	// No std::to_string() in STL...
	std::ostringstream oss;
	oss << RTN_Address(rtn) - base_address << std::endl;
	log(VL_DEBUG, "Instrumented rtn at " + oss.str());
	oss.clear();
	oss << INS_Address(RTN_InsHeadOnly(rtn)) - base_address << std::endl;
	log(VL_DEBUG, "First ins at " + oss.str());

	RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)end_of_execution, 
		IARG_UINT64, &(routines_catalog.find(RTN_Name(rtn))->second),
		IARG_THREAD_ID, IARG_END);
	RTN_Close(rtn);
}

VOID jit_run(TRACE t, VOID *v) {
	// No std::to_string() in STL...
#ifdef DEBUG
	std::ostringstream oss;
	oss << PIN_ThreadId(); 
	log(VL_DEBUG, "JIT kicked in on thr " + oss.str());
#endif
	tstatus[PIN_ThreadId()].activation_stack.clear();
	// TODO: check what actually happens during JIT compilation on multithreaded applications
}

#ifdef GET_PROCFS_INFO	
void get_base_address() {
	pid = PIN_GetPid();
	std::stringstream ss;
	ss << pid;
	std::string maps_fname = "/proc/" + ss.str() + "/maps";
	ifstream maps(maps_fname.c_str());
	// I assume the info I need is always in the first line
	// I need to parse something like 558136ca9000-558136cab000
	std::string mem_range;
	maps >> mem_range;
	maps.close();
	log(VL_DEBUG, "MRANGE: " + mem_range);
	int dash_idx = mem_range.find("-");
	mem_range = mem_range.substr(0, dash_idx);
	log(VL_DEBUG, "MRANGE: " + mem_range);
	ss.str("");
	ss << std::hex << mem_range;
	ss >> base_address;
}
#endif


/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */
int main(int argc, char * argv[])
{	
	// Initialize symbol table code, needed for rtn instrumentation
	PIN_InitSymbols();

	// Initialize pin
	if (PIN_Init(argc, argv)) 
		return usage();

	vl = (enum verbosity_levels)KnobVerbosityLevel.Value();

#ifdef GET_PROCFS_INFO	
	open_freq_handlers();
	get_base_address();
#endif

	quit_dump_thread = false;

	// No std::to_string() in STL...
	std::ostringstream oss;
	oss << base_address << std::endl;
	log(VL_DEBUG, "BASE ADDRESS " + oss.str());
	for (int t = 0; t < MAXTHREADS; t++) {
		tstatus[t].init();
	}
	
	srand(time(NULL));
	
	log(VL_INFO, "Reading table...");
	bin_reader::read("table.txt", routines_catalog);

	oss.clear();
	oss << routines_catalog.size();
	log(VL_DEBUG, oss.str() + " symbols read");
	
	log(VL_INFO, "Instrumenting the binary...");
	// Register Routine to be called to instrument rtn
	RTN_AddInstrumentFunction(instrument_function, 0);
	
	if (KnobMeasureMemory.Value())
		IMG_AddInstrumentFunction(malloc_routine, 0);

	if (KnobMeasureLocks.Value())
		IMG_AddInstrumentFunction(locking_routines, 0);
	
	// Instrument at instruction level granularity to find branch instructions
	if (KnobFindBranches.Value())
		INS_AddInstrumentFunction(instr_branches, 0);

	// Register a routine that gets called when a trace is
    	//  inserted into the codecache
	TRACE_AddInstrumentFunction(jit_run, NULL);

	// Register Fini to be called when the application exits
	PIN_AddFiniFunction(Fini, 0);

	// Prepare the dumping thread
	DUMP_LOG_PERIOD = KnobDumpPeriod.Value();
	PIN_SpawnInternalThread(&dump_logs, NULL, 0, &dlt_uid);
	log(VL_DEBUG, "Launched dumping thread");

	// Start the program, never returns
	log(VL_INFO, "Starting target program");
	PIN_StartProgram();
	return 0;
}

