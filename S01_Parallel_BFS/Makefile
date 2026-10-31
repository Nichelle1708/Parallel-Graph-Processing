CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O3 -fopenmp -Wall -Wextra
LDFLAGS  ?= -fopenmp

BIN = bin

all: $(BIN)/bfs_bench

$(BIN)/bfs_bench: src/bfs_bench.cpp src/bfs.hpp src/graph.hpp
	@mkdir -p $(BIN)
	$(CXX) $(CXXFLAGS) src/bfs_bench.cpp -o $@ $(LDFLAGS)

$(BIN)/test_bfs: tests/test_bfs.cpp src/bfs.hpp src/graph.hpp
	@mkdir -p $(BIN)
	$(CXX) $(CXXFLAGS) tests/test_bfs.cpp -o $@ $(LDFLAGS)

test: $(BIN)/test_bfs
	./$(BIN)/test_bfs

clean:
	rm -rf $(BIN)

.PHONY: all test clean
