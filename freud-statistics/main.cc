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

#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <limits>

#include "const.hh"
#include "method.hh"
#include "stats.hh"
#include "reader.hh"
#include "reader_annotations.hh"
#include "function.hh"
#include "analysis.hh"
#include "checker.hh"
#include "plotter.hh"

#include <Rinternals.h>
#include <Rembedded.h>

void printUsage(const char *argv[]) {
	std::cout << "Usage: " << argv[0] << " OPERATION [function_name] [magictype] INPUT_FILES" << std::endl;
	std::cout << std::endl << "Operations: " << std::endl;
	std::cout << "\t" << GNUPLOT << ": produce a gnuplot with values measured and the specified parameter for a specific function (" << argv[0] << " 1 MTYPE FNAME FCOUNT [\"ft1\",\"ft2\"]) PATH" << std::endl;
	std::cout << "\t" << ANNOTATION << ": (min_r2) (metric type) (symbol_name) print derived best-effort annotations for every function" << std::endl;
	std::cout << "\t" << CHECK << ": check a given performance annotation against the input data" << std::endl;
}

/**
* Loads external R source code 
*/
void source(const char *name)
{
    SEXP e;
    PROTECT(e = lang2(install("source"), mkString(name)));
    R_tryEval(e, R_GlobalEnv, NULL);
    UNPROTECT(1);
}

int main(int argc, const char *argv[]) {
	if (argc < 3) {
		printUsage(argv);
		return 0;
	}
	
	// Intialize the embedded R environment.
	int r_argc = 4;
	char exe[] = "R";
	char o1[] = "--silent";
	char o2[] = "--no-save";
	char o3[] = "--gui=none";
	char *r_argv[] = { exe, o1, o2, o3 };  
	Rf_initEmbeddedR(r_argc, r_argv);	
	source("hpi.R");
	
	int k;
	int features_count;
	int operation = atoi(argv[1]);
	double min_r2;
	metric_type mtype;
	std::string symbol_name;
	std::vector<std::string> features_index_type;
	
	if (operation == PRINT)
                k = 3; // argv[2] is the name of a function
	else if (operation == GNUPLOT) {
		features_count = atoi(argv[4]);
		if (features_count < 1) {
			utils::log(VL_ERROR, "There must be at least 1 feature!");
			exit(-1);
		}
		for (int i = 0; i < features_count; i++)
			features_index_type.push_back(argv[5 + i]);//ftype = atoi(argv[3]);
		features_index_type.push_back("gnuplot OP");
		mtype = (metric_type)atoi(argv[2]);
                k = 5 + features_count; // argv[2] fname, argv[3] magic type, argv[4] remove_deps
        }
	else if (operation == ANNOTATION || operation == CHECK) {
		k = 5;
		min_r2 = std::stod(std::string(argv[2]));
		mtype = (metric_type)atoi(argv[3]);
		symbol_name = argv[4];
	}
	else
                k = 2;
        
	if (min_r2 == 0)
		min_r2 = MIN_DET;

	utils::log(VL_INFO, "Reading data from " + std::string(argv[k]) + "... ");

	// this is the main vector, that holds all the data collected from the system
	std::map<std::string, method *> data;
	std::unordered_map<uint64_t, measure *> m_ids_map;
	reader::read_folder(argv[k], data, m_ids_map);

	if (operation == ANNOTATION) {
		unsigned int understood = 0;
		std::stringstream out_ann_file;
		if (symbol_name != "-")
			out_ann_file << "ann/" << symbol_name << ".txt";
		else
			out_ann_file << "ann/all.txt";
		std::ofstream outFile(out_ann_file.str().c_str(), std::ios::out);
		if (!outFile.is_open()) {
			utils::log(VL_ERROR, "Could not open outfile " + out_ann_file.str());
			exit(-1);
		}
		for (std::pair<std::string, method *> p: data) {
			if (symbol_name != "-" && p.first != symbol_name)
				continue;

			utils::log(VL_INFO, "Creating Performance Annotation (" + std::to_string(mtype) + ") for " + p.first);

			// TRY REGRESSIONS				
			analysis a(p.first, p.second, mtype, &m_ids_map);
			understood += a.find_regressions(min_r2);
		
			utils::log(VL_INFO, "Regressions found: " + std::to_string(p.second->performance_annotation.regressions.size()));
			plotter::plot_annotations(p.second, mtype);

			// CLUSTERING if nothing worked, fall back on kde 1d clustering
			if (p.second->performance_annotation.regressions.size() == 0) {
				analysis a(p.first, p.second, mtype, &m_ids_map);
				if (a.cluster()) 
					utils::log(VL_INFO, "Clustering succeeded!");
			}
			outFile << utils::generate_annotation_string(&(p.second->performance_annotation));
		}
		outFile.close();

	} else if (operation == GNUPLOT) {
		// TODO: remove
		for (std::pair<std::string, method *> p: data) {
			if (p.first == argv[3]) {
				utils::log(VL_INFO, "Plotting " + std::to_string(mtype) + " for method " + p.first);
				plotter::graph(p.second, "eps/" + p.first, features_index_type, mtype);
			}
		}	
	} else if (operation == CHECK) {
		// CHECK THE ANNOTATION
		std::stringstream out_check_file;
		if (symbol_name != "-")
			out_check_file << "ass/" << symbol_name << ".txt";
		else
			out_check_file << "ass/all.txt";
		std::ofstream outFile(out_check_file.str().c_str(), std::ios::out);
		if (!outFile.is_open()) {
			utils::log(VL_ERROR, "Could not open outfile " + out_check_file.str());
			exit(-1);
		}

		std::map<std::string, annotation *> m_ann_map;
		for (std::pair<std::string, method *> p: data) {
			if (symbol_name != "-" && p.first != symbol_name)
				continue;

			utils::log(VL_INFO, "Reading annotations from " + std::string(argv[k + 1]));
			std::string afname = argv[k + 1];
			afname += "/" + symbol_name + ".txt";
			reader_annotations::read_annotations_file(afname, symbol_name, m_ann_map);
			
			if (m_ann_map.find(symbol_name) == m_ann_map.end()) {
				utils::log(VL_ERROR, "Couldn't read annotation for " + symbol_name + "; skipping");
				continue;
			}			

			checker c(p.first, p.second, mtype, false, m_ann_map[p.first], &outFile);
			bool validated = c.validate_annotation();
			if (validated) 
				outFile << "Validation succeded for " << p.first << std::endl;
			else
				outFile << "Validation failed for " << p.first << std::endl;
		}
		outFile.close();
	}
}
