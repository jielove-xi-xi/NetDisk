# Makefile for diskserver project

# Compiler
CXX = g++
# Compiler flags
CXXFLAGS = -Wall -g -std=c++14 -O2


# Target executable
TARGET = mainserver
OBJS=epoller.cpp equalizer.cpp ServerHeap.cpp main.cpp

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o $(TARGET)  

clean:
	rm -f $(OBJFILES) $(TARGET)

.PHONY: all clean
