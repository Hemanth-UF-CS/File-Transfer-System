CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -pthread

.PHONY: all clean

all: server client

server: server.cpp common.h
	$(CXX) $(CXXFLAGS) -o $@ server.cpp

client: client.cpp common.h
	$(CXX) $(CXXFLAGS) -o $@ client.cpp

clean:
	rm -f server client
	rm -rf uploads/
