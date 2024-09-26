# Makefile for diskserver project

# Compiler
CXX = g++
# Compiler flags
CXXFLAGS = -Wall -g -std=c++14 -O2


# Target executable
TARGET = server
OBJS=code/timer/*.cpp code/Log/*.cpp code/buffer/*.cpp code/pool/*.cpp code/server/*.cpp code/sql/*.cpp code/tool/*.cpp code/main.cpp

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o ./bin/$(TARGET)  -lmysqlcppconn -pthread -lcrypt -lssl -lcrypto

clean:
	rm -f $(OBJFILES) $(TARGET)

.PHONY: all clean