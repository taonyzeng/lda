all: Liveness.so Available.so Reaching.so inputs

CXX = clang
CXXFLAGS = $(shell llvm-config --cxxflags) -fcolor-diagnostics -g -O0 -fPIC
OPT = opt
TEST = test-inputs

dataflow.o: Dataflow.cpp Dataflow.h
liveness.o: Liveness.cpp 
available.o: Available.cpp
reaching.o: Reaching.cpp

%.so: %.o Dataflow.o 
	$(CXX) -dylib -shared $^ -o $@
 
# TESTING
inputs : $(patsubst %.c,%.bc,$(wildcard $(TEST)/*.c)) 
 
%.tmp: %.c
	$(CXX) -O0 -emit-llvm -c $^ -o $@ 

%.bc: %.tmp
	$(OPT) -mem2reg $^ -o $@

# CLEAN
clean:
	rm -f *.o *~ *.so out         
