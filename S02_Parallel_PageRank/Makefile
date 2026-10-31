CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O3 -fopenmp -Wall -Wextra
LDFLAGS  ?= -fopenmp

BIN = bin

all: $(BIN)/pagerank_bench

$(BIN)/pagerank_bench: src/pagerank_bench.cpp src/pagerank.hpp src/graph.hpp
	@mkdir -p $(BIN)
	$(CXX) $(CXXFLAGS) -Isrc src/pagerank_bench.cpp -o $@ $(LDFLAGS)

$(BIN)/test_pagerank: tests/test_pagerank.cpp src/pagerank.hpp
	@mkdir -p $(BIN)
	$(CXX) $(CXXFLAGS) tests/test_pagerank.cpp -o $@ $(LDFLAGS)

test: $(BIN)/test_pagerank
	./$(BIN)/test_pagerank

clean:
	rm -rf $(BIN)

.PHONY: all test clean
