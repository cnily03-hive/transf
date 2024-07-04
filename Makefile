CXX = clang++

INCLUDES = -I./include -I./src

CXXFLAGS = $(INCLUDES) -static -Wall -Wextra -std=c++20
FLAGS =

DEBUG ?= 0
ifeq ($(DEBUG), 1)
	FLAGS += -g
else
	FLAGS += -O3
endif

LIBS = -lws2_32

SRC_DIR = src
BUILD_DIR = build

# Source files
CPP_SRCS = $(wildcard $(SRC_DIR)/*.cpp)
TPP_SRCS = $(wildcard $(SRC_DIR)/*.tpp)
SRCS = $(CPP_SRCS) $(TPP_SRCS)
CLIENT_SRC = transf_client.cpp
SERVER_SRC = transf_server.cpp

# Object files
CPP_OBJS = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(notdir $(CPP_SRCS)))
TPP_OBJS = $(patsubst %.tpp, $(BUILD_DIR)/%.o, $(notdir $(TPP_SRCS)))
OBJS = $(CPP_OBJS) $(TPP_OBJS)
CLIENT_OBJ = $(BUILD_DIR)/transf_client.o
SERVER_OBJ = $(BUILD_DIR)/transf_server.o

# Output executables
CLIENT_TARGET = transf_client
SERVER_TARGET = transf_server
TARGETS = $(CLIENT_TARGET) $(SERVER_TARGET)

# Default Arguments
HOST = 127.0.0.1
PORT = 3081

# Default rule
all: $(CLIENT_TARGET) $(SERVER_TARGET)

# Build and run instantly
client: $(CLIENT_TARGET)
	./$(CLIENT_TARGET) $(HOST) $(PORT) --debug

server: $(SERVER_TARGET)
	./$(SERVER_TARGET) $(PORT) --debug

# Link the executable
$(CLIENT_TARGET): $(CPP_OBJS) $(CLIENT_OBJ)
	$(CXX) $(FLAGS) $(CPP_OBJS) $(CLIENT_OBJ) -o $@ $(LIBS)

$(SERVER_TARGET): $(CPP_OBJS) $(SERVER_OBJ)
	$(CXX) $(FLAGS) $(CPP_OBJS) $(SERVER_OBJ) -o $@ $(LIBS)

# Compile source files into object files
$(CPP_OBJS): $(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(FLAGS) -c $< -o $@

$(BUILD_DIR)/transf_client.o: transf_client.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(FLAGS) -c $< -o $@

$(BUILD_DIR)/transf_server.o: transf_server.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(FLAGS) -c $< -o $@

# Create build directory
$(BUILD_DIR):
	rm -rf $(BUILD_DIR)
	mkdir $(BUILD_DIR)

# Clean generated files
clean:
	rm -rf $(BUILD_DIR) $(TARGETS)

.PHONY: all clean client server