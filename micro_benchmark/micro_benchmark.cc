/*
Copyright 2020 Daniele Rogora

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <unistd.h>
#include <cstdlib>
#include <time.h>
#include <string.h>
#include <vector>
#include <cmath>
#include <stdio.h>

#define UPPER_BOUND 300
#define STEP 2

int global_feature;

namespace whatever {
	class namespaced_abstract_class {
public:
		int namespaced_value;
		void set_val(int v) {
			namespaced_value = v;
		};

		virtual int get_val() {
			return namespaced_value;
		};
	};

	class namespaced_derived_class: public namespaced_abstract_class {
		virtual int get_val() {
			return namespaced_value / 2;
		}
	};
}

class abstract_class_1 {
private:
	virtual void some_method_1() const = 0;
};

class abstract_class_2 {
	virtual void some_method_2() = 0;
};

class derived_class : public abstract_class_1, public abstract_class_2 {
private:
	int private_value;
public:
	derived_class(int x) { private_value = x; };
	void some_method_1() const { };
	void some_method_2() { };
	int get_pv() const { return private_value; };
};

class derived_class2 : public abstract_class_2 {
private:
public:
	int private_value2;
	derived_class2() { private_value2 = 0; };
	derived_class2(int x) { private_value2 = x; };
	virtual void some_method_2() { };
	virtual int get_pv() const { return private_value2; };
};

class grand_derived_class2 : public derived_class2 {
private:
public:
	grand_derived_class2(int x) { private_value2 = x; };
	virtual void some_method_2() { };
	int get_pv() const { return private_value2; };
};

class fit_in_register {
private:
	int single_int;
	int another_int;
	//int yet_another_int;

public:
	int get_int() {
		return single_int;
	}

	void set_int(int n) {
		single_int = n;
		another_int = 21;
		//yet_another_int = 87;
	}
};

class basic_class {
private:
	int private_field;
	int other_private_field;
	fit_in_register * fir;

public:
	unsigned int arr[2][10];
	int useless;

	basic_class(int pf) {
		private_field  = pf; 
	};

	void set_pf(int pf) {
		private_field  = pf; 
	};

	void set_other_pf(int opf) {
		other_private_field = opf;
	};

	int get_private_field() {
		return private_field;
	};

	void set_fir(fit_in_register * f) {
		fir = f;		
	};

	void set_useless(int uless) {
		useless = uless;
	};

	int __attribute__ ((noinline)) test_linear_classfield() {
		for (int i = 0; i < other_private_field; i++)
			usleep(87);
		return 5;
	}
};

struct basic_structure {
	unsigned int useless;
	unsigned int useful;
};

enum command {
	CMD_CONSTANT = 0,
	CMD_LINEAR,
	CMD_QUAD
};

/******
 * NUMERIC
 *******/
void __attribute__ ((noinline)) test_linear_int_pointer(int * t) {
	for (int i = 0; i < *t; i++) {
		usleep(250);
	}
	//malloc(t);
}

void __attribute__ ((noinline)) test_linear_int(int t) {
	for (int i = 0; i < t; i++) {
		usleep(250);
	}
	//malloc(t);
}

void __attribute__ ((noinline)) test_linear_float(float t) {
	for (int i = 0; i < (int)t; i++) {
		usleep(250);
	}
}

void __attribute__ ((noinline)) test_quad_int(int t) {
	for (int i = 0; i < t; i++) {
		usleep(t);
	}
}

void __attribute__ ((noinline)) test_nlogn_int(int t) {
	for (int i = 0; i < log2(t); i++) {
		usleep(t);
	}
}

void __attribute__ ((noinline)) test_quad_int_wn(int t, int nlevel) {
	for (int i = 0; i < t; i++) {
		unsigned int noise = rand() % t * 100;
		usleep(t + noise);
	}
}

/******
 * STRINGS
 ******/
int __attribute__ ((noinline)) test_linear_charptr(char * str) {
	for (int i = 0; i < strlen(str); i++) {
		usleep(100);
	}
	return 5;
}

/******
 * STRUCTS
 ******/
int __attribute__ ((noinline)) test_linear_structs(struct basic_structure * bs) {
	for (int i = 0; i < bs->useful; i++) {
		usleep(150);
	}
	return 5;
}

/******
 * CLASSES
 ******/
void __attribute__ ((noinline)) test_linear_classes(basic_class * bc) {
	int pf = bc->get_private_field();
	for (int i = 0; i < pf; i++) {
		usleep(150);
	}
}

void __attribute__ ((noinline)) test_linear_fitinregister(fit_in_register fir) {
	int pf = fir.get_int();
	for (int i = 0; i < pf; i++) {
		usleep(150);
	}
}


/******
 * BRANCHES
 ******/
void __attribute__ ((noinline)) test_linear_branches(int a, int b, int c) {
	if (a > 10) {
		for (int i = 0; i < b; i++) {
			usleep(b);
		}
	}
	else {
		for (int i = 0; i < c; i++) {
			usleep(450);
		}
	}
}

void __attribute__ ((noinline)) test_linear_branches_one_f(int a, int b, int c) {
	if (a < 10) {
		for (int i = 0; i < 10 - a; i++) {
			usleep(400);
		}
	}
	else {
		usleep(4000);
		for (int i = 0; i < a - 10; i++) {
			usleep(400);
		}
	}
}

/******
 * ENUMS
 ******/
int __attribute__ ((noinline)) test_multi_enum(enum command c, int a) {
	switch (c) {
		case CMD_CONSTANT:
			usleep(100000);
			break;
		case CMD_LINEAR:
			for (int i = 0; i < a; i++) {
				usleep(100);
			}
			break;
		case CMD_QUAD:
			for (int i = 0; i < a; i++) {
				usleep(a);
			}
			break;
	}
	return 5;
}

#if 0
/******
 * COMPOSITION
 ******/
void __attribute__ ((noinline)) first_component(int a) {
	for (int i = 0; i < a; i++) {
		usleep(450);
	}
}

void __attribute__ ((noinline)) second_component(int a) {
	//for (int i = UPPER_BOUND; i >= UPPER_BOUND - a; i--) {
		usleep(rand() % 500000);
	//}
}

int __attribute__ ((noinline)) test_linear_composition(int a, int b) {
	first_component(a);
	second_component(b);
	return 5; // this is needed, because Pin would not catch the exit point otherwise
}
#endif

/******
 * GLOBAL FEATURE
 ******/
int __attribute__ ((noinline)) test_global_feature() {
	for (int i = 0; i < global_feature; i++) {
		usleep(70);
	}
	return 5;
}

/******
 * LOOPS
 ******/
void __attribute__ ((noinline)) test_loop(int array[], int size) {
	for (int i = 0; i < size; i++) {
		if (array[i] < 7) 
			usleep(100);
	}
}

/******
 * CLUSTERING
 ******/
int __attribute__ ((noinline)) test_random_clustering(int a) {
#if 0
	struct timespec ts = {.tv_sec = 0, .tv_nsec = 200};
	int rnd = rand() % 100;
	if (rnd < 30)
		ts.tv_nsec = 400000;
	else if (rnd < 70)
		ts.tv_nsec = 600000;
	else
		ts.tv_nsec = 100000;
	nanosleep(&ts, 0);
#else	
	int rnd = rand() % 100;
	if (rnd < 30)
		usleep(900);
	else if (rnd < 70)
		usleep(600);
	else
		usleep(300);
#endif
	return 5;
}

/******
 * MAIN COMPONENT
 ******/
int __attribute__ ((noinline)) test_interaction_linear_quad(int a, int b) {
	for (int i = 0; i < a; i++)
		usleep(b*b);
	return 5;
}

/******
 * MAIN COMPONENT
 ******/
int __attribute__ ((noinline)) test_linear_main_component(int a) {
	int p = rand() % 100;
	if (p < 25)
		usleep(rand() % 1500 + 10000);
	else if (p >= 25 && p < 50)
		usleep(rand() % 100);
	else {
		for (int i = 0; i < a; i++) {
			usleep(1000);
		}
	}
	return 5;
}

/******
 * C++ TEMPLATES
 ******/
int __attribute__ ((noinline)) test_linear_vector(std::vector<int> * t) {
	for (int i = 0; i < t->size(); i++) {
		usleep(250);
	}
	return 5;
}

int __attribute__ ((noinline)) test_linear_farray(unsigned int arr[2][10]) {
	for (int j = 0; j < 2; j++) {
		for (int i = 0; i < 10; i++) {
			usleep(arr[j][i]);
		}
	}
	return 5;
}

/******
 * C++ POLYMORPHISM
 ******/
int __attribute__ ((noinline)) test_derived_class(const abstract_class_1 &c) {
	const derived_class &dc = dynamic_cast<const derived_class &>(c);
	usleep(dc.get_pv());
	return 5;
}

int __attribute__ ((noinline)) test_grand_derived_class2(const abstract_class_2 *c) {
	const grand_derived_class2 *gdc = dynamic_cast<const grand_derived_class2 *>(c);
	if (gdc)
		usleep(gdc->get_pv());
	const derived_class *dc = dynamic_cast<const derived_class *>(c);
	if (dc)
		usleep(2000 - dc->get_pv());
	return 5;
}

/******
 * C++ NAMESPACES 
 ******/
void __attribute__ ((noinline)) test_namespace(whatever::namespaced_abstract_class * nc) {
	for (int i = 0; i < nc->get_val(); i++) {
		usleep(250);
	}
	//malloc(t);
}

void run_validation() {
	global_feature = 55;

	char * foo_str = (char *)malloc(sizeof(char) * UPPER_BOUND);
	struct basic_structure bs;
	basic_class * bc = new basic_class(0);
	fit_in_register * fir = new fit_in_register();
	std::vector<int> * vec = new std::vector<int>();
	// NUMERIC
	test_linear_int(1);
	int pp = 111;
	test_linear_int_pointer(&pp);
	test_linear_float((float)2.0);
	test_quad_int(3);
	test_nlogn_int(3);
	test_quad_int_wn(4, 5);

	// STRING
	memset(foo_str, 'a', 6);
	foo_str[7] = '\0';
	test_linear_charptr(foo_str);

	// STRUCTS
	bs.useless = 7;
	bs.useful = 8;
	test_linear_structs(&bs);

	// CLASSES
	for (int j = 0; j < 2; j++) 
		for (int i = 0; i < 10; i++)
			bc->arr[j][i] = 1987;
	bc->set_pf(9);
	bc->set_useless(10);
	bc->set_other_pf(11);
	fir->set_int(12);
	bc->set_fir(fir);
	test_linear_classes(bc);
	bc->test_linear_classfield();
	test_linear_fitinregister(*fir);
	
	// BRANCHES
	test_linear_branches(13, 14, 15);
	test_linear_branches_one_f(16, 17, 18);

	// ENUMS
	test_multi_enum(CMD_LINEAR, 19);
	
#if 0
	// COMPOSITION
	test_linear_composition(rand() % 1000, rand() % 1000);
#endif
	
	// CLUSTERING
	test_random_clustering(20);

	// MAIN COMPONENT
	test_linear_main_component(21);

	// C++ TEMPLATES
	vec->resize(22);
	test_linear_vector(vec);

	// POLYMORPHISM
	derived_class dc(1987);
	test_derived_class(dc);

	grand_derived_class2 dc2(19872);
	test_grand_derived_class2(&dc2);

	// FIXED SIZE ARRAYS
	unsigned int arr[2][10];
	for (int j = 0; j < 2; j++) 
		for (int i = 0; i < 10; i++)
			arr[j][i] = 17;
	test_linear_farray(arr);

	// NAMESPACES
	whatever::namespaced_derived_class nc;
	nc.set_val(5);
	test_namespace(&nc);

	// GLOBALS
	global_feature = 24;
	test_global_feature();
}

int main(int argc, char * argv[]) {
	if (argc > 1) {
		if (strcmp(argv[1], "--test_instrumentation") == 0) {
			// Run with known values
			run_validation();
			// The first run should be not valid because of JIT
			run_validation();
			return 0;
		} else {
			printf("Unhandled parameter: %s\n", argv[1]);
			return -1;
		}
	}
	global_feature = 0;
#if 0
	// WARMUP
	test_linear_int(UPPER_BOUND);
	test_quad_int(UPPER_BOUND);
	test_quad_int_wn(UPPER_BOUND, 100);
	
#endif

	srand(time(NULL));
	char * foo_str = (char *)malloc(sizeof(char) * UPPER_BOUND);
	struct basic_structure bs;
	basic_class * bc = new basic_class(0);
	fit_in_register * fir = new fit_in_register();
	std::vector<int> * vec = new std::vector<int>();
	printf("\n");
	bool flip = 0;
	for (int t = 0; t <= UPPER_BOUND; t += STEP) {
		printf("\rStarting step %d/%d", t, UPPER_BOUND);
		fflush(stdout);
		// NUMERIC
		test_linear_int(t);
		test_linear_int_pointer(&t);
		test_linear_float((float)t);
		test_quad_int(t);
		test_nlogn_int(50*t);
		test_quad_int_wn(t, 100);

		// STRING
		if (t > 0)
			memset(foo_str + t - STEP, 'a', STEP);
		foo_str[t] = '\0';
		test_linear_charptr(foo_str);

		// STRUCTS
		bs.useless = rand();
		bs.useful = t;
		test_linear_structs(&bs);

		// CLASSES
		bc->set_pf(t);
		bc->set_useless(rand());
		test_linear_classes(bc);
		bc->set_other_pf(rand() % 50);
		bc->test_linear_classfield();
		fir->set_int(rand() % 50);
		test_linear_fitinregister(*fir);
		
		// BRANCHES
		test_linear_branches(rand() % 21, rand() % 1000, rand() % 1000);
		test_linear_branches_one_f(rand() % 21, rand() % 1000, rand() % 1000);

		// ENUMS
		test_multi_enum((command)(t % 3), rand() % 1000);
		
#if 0
		// COMPOSITION
		test_linear_composition(rand() % 1000, rand() % 1000);
#endif
		
		// CLUSTERING
		test_random_clustering(rand() % 200);

		// MAIN COMPONENT
		test_linear_main_component(rand() % 10);

		// INTERACTION
		test_interaction_linear_quad(rand() % 20, rand() % 20);
	
		// C++ TEMPLATES
		vec->push_back(rand() % 1000);
		test_linear_vector(vec);

		// FIXED SIZE ARRAYS
		unsigned int arr[2][10];
		for (int j = 0; j < 2; j++) 
			for (int i = 0; i < 10; i++)
				arr[j][i] = rand() % + (150 * (t+1));
		test_linear_farray(arr);


		// GLOBALS
		global_feature = rand() % 100;
		test_global_feature();

		// LOOP
		int a[10];
		for (int aa = 0; aa < 10; aa++)
			a[aa] = rand() % 20;
		test_loop(a, 10);

		// POLYMORPHISM
		derived_class dc(rand() % 1987);
		test_derived_class(dc);

		abstract_class_2 * dc2;
		if (flip) 
			dc2 = new grand_derived_class2(rand() % 1987);
		else
			dc2 = new derived_class(rand() % 1987);
			
		test_grand_derived_class2(dc2);
		flip = !flip;
		//whatever::namespaced_abstract_class nc;
		//nc.set_val(rand() % 21);
		//test_namespace(&nc);
	}
	
	free(foo_str);
}
