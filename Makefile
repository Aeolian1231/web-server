# ---- C++ epoll server (Milestone 1) ----
CXX       ?= g++
CXXFLAGS  ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread
CPP_INCS  := -Iinclude_cpp
CPP_SRCS  := src_cpp/webserver_epoll.cpp src_cpp/epoller.cpp
CPP_OBJS  := $(CPP_SRCS:.cpp=.o)
CPP_BIN   := bin/webserver_epoll

.PHONY: epoll_server clean_epoll

epoll_server: $(CPP_BIN)

$(CPP_BIN): $(CPP_OBJS) | bin
	$(CXX) $(CXXFLAGS) $(CPP_OBJS) -o $@

src_cpp/%.o: src_cpp/%.cpp | bin
	$(CXX) $(CXXFLAGS) $(CPP_INCS) -c $< -o $@

bin:
	mkdir -p bin

clean_epoll:
	rm -f $(CPP_OBJS) $(CPP_BIN)