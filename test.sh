#!/bin/bash
export R_HOME=/usr/lib/R

set -e

echo "Cleaning up previous runs..."
rm -rf test/ann
rm -rf test/symbols
rm -rf micro_benchmark_analysis/eps
rm -rf micro_benchmark_analysis/plots
rm -rf micro_benchmark_analysis/ann
rm -rf micro_benchmark_analysis/symbols
rm -rf freud-pin/symbols
cp freud-statistics/freud-statistics micro_benchmark_analysis
cp freud-statistics/hpi.R micro_benchmark_analysis

echo "Creating the instrumentation..."
./freud-dwarf/freud-dwarf micro_benchmark/micro_benchmark --max-depth=4 >> test_log
mv feature_processing.cc freud-pin/ 
mv table.txt freud-pin/ 
cd freud-pin/
make clean > /dev/null
make &>> test_log
mkdir symbols

echo "Running test program for instrumentation..."
../pin/pin -t obj-intel64/freud-pin.so -- ../micro_benchmark/micro_benchmark --test_instrumentation &>> test_log
mv symbols ../test/
cd ../

echo "Analyzing instrumentation results..."
cd test
./test --test-instr

echo "Running test program for the statistical analysis..."
rm -rf symbols # clean the logs for the test of the instrumentation
cd ../freud-pin/
mkdir symbols
../pin/pin -t obj-intel64/freud-pin.so -- ../micro_benchmark/micro_benchmark #&>> test_log
mv symbols ../micro_benchmark_analysis/

echo "Performing statistical analysis..."
cd ../
cp freud-statistics/freud-statistics micro_benchmark_analysis
cp freud-statistics/hpi.R micro_benchmark_analysis
cd micro_benchmark_analysis/
mkdir eps
mkdir plots
make &>> make_log

echo "Analyzing statistical results..."
mv ann ../test/ 
cd ../test
./test --test-stats

echo "Done!"
echo "You can find the plots for the benchmarks in micro_benchmark_analysis/eps"
