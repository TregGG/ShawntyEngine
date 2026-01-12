CXX := g++

TARGET := framework

OBJ_DIR := obj
BIN_DIR := bin

# Source files
SRCS := main.cpp \
	sanitycheck.cpp \
        external/glad/src/glad.c \
        $(wildcard core/*.cpp) \
        $(wildcard levels/*.cpp)\
	$(wildcard render/*.cpp)\
	$(wildcard assets/*.cpp)\
	$(wildcard objects/*.cpp)

# Object files
OBJS := $(SRCS:.cpp=.o)
OBJS := $(OBJS:.c=.o)
OBJS := $(patsubst %,$(OBJ_DIR)/%,$(OBJS))

# Compiler & linker flags
CXXFLAGS := -std=c++17 -Wall -Wextra -Icore -Ilevels -Iexternal/glad/include -Iexternal/glm
LDFLAGS  := $(shell pkg-config --cflags --libs glfw3) -lGL

# Final binary
BIN := $(BIN_DIR)/$(TARGET)

# ---------------- RULES ----------------

all: $(BIN)

$(BIN): $(OBJS) | $(BIN_DIR)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

# Compile C++ files
$(OBJ_DIR)/%.o: %.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile C files (for glad.c)
$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create directories
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Clean
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
