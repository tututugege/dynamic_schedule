CXXSRCS = ./*.cpp

CXXFLAGS := -I./include/ 

CXXFLAGS += $(shell python3 -m pybind11 --include)

CXXFLAGS += -shared -std=c++11 -fPIC

all-shared:
	g++ $(CXXFLAGS) $(CXXSRCS) -o cpu_simulator$(shell python3-config --extension-suffix)

all:
	g++ -I./include/ $(CXXSRCS) -g

run: all
	./a.out

gdb: all
	gdb ./a.out

clean:
	rm -rf a.out log

.PHONY:all run gdb

