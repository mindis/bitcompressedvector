SHELL = /bin/bash
BUILD_DIR=build

#CC=/usr/gcc-4.8-20120930/bin/g++-4.8-20120930
#CC=clang++
CXX ?= clang++
CXXFLAGS= -mssse3 -msse4.1 -m64 -std=c++0x  -Weffc++

FILES=main.cc test.cc

all: bcv.h test.cc test.h main.cc 
	mkdir -p $(BUILD_DIR)
	$(CXX) -o $(BUILD_DIR)/main $(FILES) $(CXXFLAGS) -g2 -O0 -ggdb
	$(CXX) -o $(BUILD_DIR)/main_opt $(FILES) -DNDEBUG $(CXXFLAGS) -funroll-loops -O3 -g -mtune=native 

get-deps:
	echo "Done"

profile:
	$(CC) -O3 -o $(BUILD_DIR)/main_opt $(FILES) -g2 -DNDEBUG -lprofiler $(CXXFLAGS)
	CPUPROFILE_FREQUENCY=1000 CPUPROFILE=/tmp/bcv.prof ./$(BUILD_DIR)/main_opt 100000000
	pprof --pdf ./$(BUILD_DIR)/main_opt /tmp/bcv.prof > bcv.pdf

gen: decompress.h decompress2.h

decompress.h: tpl/all.tpl code_gen.py 
	python code_gen.py > decompress.h

decompress2.h: tpl/vertical.tpl code_gen_vertical.py
	python code_gen_vertical.py > decompress2.h

papi:
	g++ -O3 -o $(BUILD_DIR)/main_opt $(FILES) -g2 -DNDEBUG -lpapi -DUSE_PAPI_TRACE -lpthread

test: all
	./$(BUILD_DIR)/main 100000
	
release: gen
	$(RM) -Rf pkg/bcv
	mkdir -p pkg/bcv
	cp decompress.h pkg/bcv
	cp decompress2.h pkg/bcv
	cp bcv.h pkg/bcv
	tar -C pkg -zcvf bcv.tgz bcv


clean:
	$(RM) -Rf pkg bcv.tgz $(BUILD_DIR)

.PHONY: decompress.h decompress2.h
