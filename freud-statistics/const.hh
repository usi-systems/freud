#ifndef CONST_HH_DEFINED
#define CONST_HH_DEFINED

// Max degree for a regression; internal parameter, not to be modified alone
#define MAX_DEGREE 2

enum filter_type {
	FLT_LE = 0,
	FLT_GT,
	FLT_BT,
	FLT_EQ,
	FLT_DF,
	FLT_NONE
};

enum operations {
	PRINT,
	GNUPLOT,
	PCC,
	ANNOTATION,
	DEPENDENCIES,
	CHECK
};

enum feature_type {
	FT_BOOL = 0,
	FT_INT,
	FT_COLLECTION,
	FT_COLLECTION_AGGR,
	FT_STRING, // and also mysqllexstring are caught here
	FT_PATH,
	FT_NASCII,
	FT_SIZE,
	FT_RESOURCE,
	FT_ENUM,
	FT_BRANCH_EXEC,
	FT_BRANCH_ISTRUE,
	FT_UNKNOWN // either not existing, or not found/decided yet
};

enum computation_state {
	ST_TOT,
	ST_STDDEV,
	ST_COVARIANCE,
	ST_REGRESSION,
	ST_CLUSTER
};

#define METRICS_COUNT 6

enum metric_type {
	MT_TIME = 0,
	MT_MEM,
	MT_LOCK,
	MT_WAIT,
	MT_PAGEFAULT_MINOR,
	MT_PAGEFAULT_MAJOR
};

#endif
