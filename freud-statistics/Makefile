.PHONY:	all
all: freud-statistics 


OBJECTS = utils.o stats.o reader.o reader_annotations.o regression.o plotter.o analysis.o checker.o main.o

CXXFLAGS = -g -O3 -std=c++11 -I/usr/share/R/include
LIBS = -lR

freud-statistics: $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@
	
.PHONY: clean
clean: 
	rm -f *.o freud-statistics
