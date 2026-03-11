CXX := g++

TARGET := framework

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

SRCS := main.cpp \
        external/glad/src/glad.c \
        $(wildcard core/*.cpp) \
        $(wildcard levels/*.cpp) \
        $(wildcard render/*.cpp) \
        $(wildcard assets/*.cpp) \
        $(wildcard objects/*.cpp) \
        $(wildcard objects/components/*.cpp) \
        $(wildcard test/*.cpp)

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
              -Icore -Ilevels -Iexternal/glad/include -Iexternal/glm

ifeq ($(BUILD),console)
CXXFLAGS := $(BASE_FLAGS) -DENGINE_DEBUG -DENGINE_LOG_CONSOLE
else ifeq ($(BUILD),file)
CXXFLAGS := $(BASE_FLAGS) -DENGINE_DEBUG -DENGINE_LOG_FILE
else ifeq ($(BUILD),release)
CXXFLAGS := $(BASE_FLAGS) -O2 -DENGINE_RELEASE
else
CXXFLAGS := $(BASE_FLAGS) -DENGINE_DEBUG -DENGINE_LOG_BOTH
endif

LDFLAGS := $(shell pkg-config --cflags --libs glfw3) -lGL

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