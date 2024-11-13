# Compiler and flags
CXX := clang++
CXXFLAGS := -std=c++14 -O3

# LLVM configuration
LLVM_CXXFLAGS := $(shell llvm-config --cxxflags)
LLVM_LDFLAGS := $(shell llvm-config --ldflags --system-libs --libs core)

all: knownbits_rotateleft

knownbits_rotateleft: main.cpp
	$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS) -lunwind main.cpp $(LLVM_LDFLAGS) -o knownbits_rotateleft.o

# Clean up build artifacts
clean:
	rm -f main.o
