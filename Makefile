# ---- C++ epoll Reactor + threadpool server (Stage 2) ----
CXX       ?= g++
CXXFLAGS  ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread
CPP_INCS  := -Iinclude_cpp

# Add Stage 2 sources (epoll entry + epoller + buffer/http/conn + threadpool + notifier)
CPP_SRCS := \
  src_cpp/webserver_epoll.cpp \
  src_cpp/epoller.cpp \
  src_cpp/timer_heap.cpp \
  src_cpp/buffer.cpp \
  src_cpp/http.cpp \
  src_cpp/http_response.cpp \
  src_cpp/conn.cpp \
  src_cpp/threadpool.cpp \
  src_cpp/notifier.cpp \
  src_cpp/async_logger.cpp

CPP_OBJS  := $(CPP_SRCS:.cpp=.o)
CPP_BIN   := bin/webserver_epoll

.PHONY: all epoll_server clean_epoll

all: epoll_server

epoll_server: $(CPP_BIN)

$(CPP_BIN): $(CPP_OBJS) | bin
	$(CXX) $(CXXFLAGS) $(CPP_OBJS) -o $@

src_cpp/%.o: src_cpp/%.cpp | bin
	$(CXX) $(CXXFLAGS) $(CPP_INCS) -c $< -o $@

bin:
	mkdir -p bin

clean_epoll:
	rm -f $(CPP_OBJS) $(CPP_BIN)