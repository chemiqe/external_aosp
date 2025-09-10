# Makefile for test_camera Linux ARM64

# Program name
TARGET = test_camera

SRCS = test_camera.cpp

# CXX = aarch64-linux-gnu-g++
CXX = g++

CXXFLAGS = -Wall -O2

LDFLAGS = 

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

clean:
	rm -f $(TARGET) *.o
