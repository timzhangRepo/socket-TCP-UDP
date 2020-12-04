CXX=g++
CXXFLAGS=-g -Wall -std=c++11
LDLIBS=-lpthread
EXES=servermain serverA serverB client

all: $(EXES)

clean:
	rm -rf $(EXES)

.PHONY: clean

