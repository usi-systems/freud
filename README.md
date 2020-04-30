# *FREUD*
This is the main repository for Freud, originally described in the publication 
> Analyzing System Performance with Probabilistic Performance Annotations 
> D. Rogora, A. Carzaniga A. Diwan, M. Hauswirth, R. Soulé
> In Eurosys'20: Proceedings of the Fifteenth European Conference on Computer Systems. Heraklion, Crete, Greece. April 2020.

Freud consists of 3 different components: **freud-dwarf**, **freud-pin**, and **freud-statistics**.

**freud-dwarf**
This is the component that creates the custom Pin tool that instruments the target program.

*Compilation*
```sh
cd freud-dwarf
make
```
**freud-pin**
This component consists of the shared part of the Pin tool that performs the instrumentation.
It cannot be used as is, but must be recompiled whenever the instrumentation target changes.
Trying to compile this component without an appropriate *feature_processing.cc* will fail.

*Setup*
```sh
download the latest Intel Pin for your platform
extract the downloaded archive to the root of this repository and rename the root folder of Pin to "pin"
cd freud-pin
make
```
Pin can be found at https://software.intel.com/en-us/articles/pin-a-binary-instrumentation-tool-downloads

**freud-statistics**
This component performs the statistical analysis on the data produced by the Pin tool. It depends on the R statistical package, so this must be installed (with development headers) on your computer.
On Ubuntu, install the packages *r-base*, and *r-base-dev*. Also, adjust the include location for the R headers in the freud-statistics/Makefile, if necessary.

*Compilation*
```sh
cd freud-statistics
make
```
***TEST***
This repository includes some tests to help getting started with Freud. For the test, there are two additional components: **micro_benchmark**, and **test**
**Once *all* the components are prepared correctly, it is suggested to run the test.sh script in the root folder of the repository, to see if everything is working.**
The expected output of test.sh is "All tests completed successfully!" for both the instrumentation and statistical analysis.
Make sure that the *R_HOME* environment variable in test.sh is defined correctly for your system!

**micro_benchmark**
This is a set of basic functions for which we suppose to know the expected performance. See section 5 of the paper for more details.

*Compilation*
```sh
cd micro_benchmark
make
```

**test**
This is a program which parses the performance annotations produced for the *micro_benchmark*, and acts as an oracle to validate them.

*Compilation*
```sh
cd test 
make
```

# HOW TO PRODUCE PERFORMANCE ANNOTATIONS FOR YOUR FAVOURITE C/C++ PROGRAM

Notice that in this document the "target" or "instrumented" program refers to the program for which we want to create performance annotations (e.g. mysql, envoy...) 

The end-to-end process has 4 main steps: 
1. COMPILE THE TARGET PROGRAM WITH gcc/g++ WITH DEBUGGING SYMBOLS. The binary that you want to instrument must contain debugging symbols, possibly respecting the DWARF standard.
2. CREATE THE PINTOOL. Decide which symbols to instrument, and generate the features extraction code
3. RUN THE APPLICATION WITH THE PINTOOL. Use the output of phase 2 to compile a custom Pin tool, and use it to run the target application with a "relevant" workload
4. RUN THE STATISTICAL ANALYSIS. Use the output of phase 3 to run the statistical analysis tool

**COMPILE THE TARGET PROGRAM WITH DEBUGGING SYMBOLS**
With gcc this means adding the -g -gstrict-dwarf flags to the compilation flags. How to do this depends on the build system. 

**CREATE THE PINTOOL**
This steps requires the *create-instrumentation* tool, which is created in the freud-dwarf directory.
To generate the required information run
```sh
./freud-dwarf /path/to/the/binary
```
It is recommended to use the --sym-wl=symbol_name option to specify a comma separated list of symbols to instrument. The default is to instrument *every* symbol.
The name of the symbols are specified through their C++ mangled names.
For example, to instrument "int __attribute__ ((noinline)) test_linear_structs(struct basic_structure * bs)" in the micro_benchmark, you can:
```sh
nm -g micro_benchmark | grep test_linear_structs # "_Z19test_linear_structsP15basic_structure"
./create-instrumentation micro_benchmark --sym_wl=_Z19test_linear_structsP15basic_structure
```
The output of this phase is a pair of files: "table.txt", and "feature_processing.cc"

TODO: in the current version of Freud there is a little bug for which this procedure does not work with local symbols (lowercase "t" in nm). This will be addressed soon.

**RUN THE APPLICATION WITH THE PINTOOL**
This step requires pin, that in turn uses a custom pintool that is compiled using the "feature_processing.cc" file created in the previous step.
```sh
cp table.txt feature_processing.cc freud-pin/
cd freud-pin
make clean; make
../pin -t obj-intel64/freud-pin.so --pin-tool-arguments -- /path/to/the/target/binary
```
You can append any command line parameter for the target program to the end of the string.
The freud-pin tool provides different command line arguments. You can have a description passing the -h parameter to the PinTool (before -- in the invocation line).
When the target program exits, the Pin tools creates a "symbols" directory, containing one subfolder for each symbol. Each subfolder contains binary data.

**RUN THE STATISTICAL ANALYSIS**
This step requires the analysis tool, which is created in the freud-statistics directory. The analysis tool has many command line parameters, which are described by the help message that it prints when executed without parameters. For example, the following command creates performance annotations (3) with the default R2 threshold (0) for the *time* metric (0) for the "_Z20test_linear_branchesiii" symbol, whose binary logs are in the given directory.
```sh
./freud-statistics 3 0 0 _Z20test_linear_branchesiii symbols/_Z20test_linear_branchesiii/
```
The output of the analysis is in the _eps_ (for the plots) and _ann_ (for the text annotations) directories.

***Contributions***
This repository bundles a modified version of libelfin [https://github.com/aclements/libelfin]. Thanks to the original authors.

