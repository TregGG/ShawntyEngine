CXX := g++

TARGET := framework

# Enable parallel execution by default based on number of system cores
MAKEFLAGS += -j$(shell nproc)

# --------------------------------------------------
# Build mode
# --------------------------------------------------

BUILD ?= debug

# --------------------------------------------------
# Directories
# --------------------------------------------------

ifeq ($(BUILD),release)
OBJ_DIR := release/obj
BIN_DIR := release
else
OBJ_DIR := obj
BIN_DIR := bin
endif

BIN := $(BIN_DIR)/$(TARGET)

# --------------------------------------------------
# Source files
# --------------------------------------------------

# Recursive wildcard function
rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

SRCS := main.cpp \
        external/glad/src/glad.c \
        $(wildcard external/enet/*.c) \
        $(call rwildcard,core levels render assets objects test services serialization scripting,*.cpp)

# --------------------------------------------------
# Object files
# --------------------------------------------------

OBJS := $(SRCS:.cpp=.o)
OBJS := $(OBJS:.c=.o)
OBJS := $(patsubst %,$(OBJ_DIR)/%,$(OBJS))

# --------------------------------------------------
# Compiler flags
# --------------------------------------------------

BASE_FLAGS := -std=c++17 -Wall -Wextra -g \
              -Icore -Ilevels -Iexternal/glad/include -Iexternal/glm -Iexternal/enet/include \
              -Iexternal \
              $(shell pkg-config --cflags freetype2) \
              $(shell pkg-config --cflags python3-embed) \
              $(shell python3 -m pybind11 --includes)

ifeq ($(BUILD),console)
CXXFLAGS := $(BASE_FLAGS) -DENGINE_DEBUG -DENGINE_LOG_CONSOLE
else ifeq ($(BUILD),file)
CXXFLAGS := $(BASE_FLAGS) -DENGINE_DEBUG -DENGINE_LOG_FILE
else ifeq ($(BUILD),release)
CXXFLAGS := $(BASE_FLAGS) -O2 -DENGINE_RELEASE
else
CXXFLAGS := $(BASE_FLAGS) -DENGINE_DEBUG -DENGINE_LOG_BOTH
endif

LDFLAGS := $(shell pkg-config --cflags --libs glfw3 freetype2) -lGL \
           $(shell pkg-config --libs python3-embed)

# --------------------------------------------------
# Default target
# --------------------------------------------------

all: $(BIN)

# --------------------------------------------------
# Link
# --------------------------------------------------

$(BIN): $(OBJS) | $(BIN_DIR)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

# --------------------------------------------------
# Compile C++ files
# --------------------------------------------------

$(OBJ_DIR)/%.o: %.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# --------------------------------------------------
# Compile C files (glad)
# --------------------------------------------------

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# --------------------------------------------------
# Create directories
# --------------------------------------------------

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# --------------------------------------------------
# Clean
# --------------------------------------------------

clean:
	rm -rf obj bin release

# --------------------------------------------------
# Dependency files
# --------------------------------------------------

-include $(OBJS:.o=.d)