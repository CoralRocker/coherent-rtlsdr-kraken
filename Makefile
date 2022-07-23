# g++ main.cc ccoherent.cc crtlsdr.cc cdsp.cc console.cc csdrdevice.cc cpacketizer.cc ccontrol.cc -o coherentrtlsdr 


# Define Default Options for the compiler and linker
CXX=g++
CXXFLAGS=--std=c++20 -Wall -ggdb -pedantic -O0 -I include/ 
LDFLAGS=-L rtl-sdr/src/  -lm -lpthread -lrtlsdr -lvolk -lfftw3f -lzmq -lreadline

# Source and Build Directories
BUILD_DIR := build
SRC_DIR := src

# Find all source code files except main
SRCS := $(shell find $(SRC_DIR) -name '*.cc' -and -not -name 'main.cc')

# Create an object file for each source file (except main)
OBJS := $(SRCS:src/%.cc=$(BUILD_DIR)/%.o)

# Define what to compile into
TARGET := coherentrtlsdr

# Default target
all: $(TARGET)

clean:
	rm -rf $(BUILD_DIR)
	rm $(TARGET)

# Rule for building all object files. 
# No linking is done here
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cc
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule for building target.
# Depends on all object files.
# Linking happens here.
$(TARGET): $(SRC_DIR)/main.cc $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

save2bin: Save2Bin/save.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
