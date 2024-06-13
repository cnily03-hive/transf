CXX = g++

INCLUDES = -I./include -I./src

CXXFLAGS = $(INCLUDES) -Wall -Wextra -std=c++20

LIBS = -lws2_32

SRC_DIR = src
BUILD_DIR = build

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
CLIENT_SRC = udp_client.cpp
SERVER_SRC = udp_server.cpp

# Object files
OBJS = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(notdir $(SRCS)))
CLIENT_OBJ = $(BUILD_DIR)/udp_client.o
SERVER_OBJ = $(BUILD_DIR)/udp_server.o

# Output executables
CLIENT_TARGET = udp_client
SERVER_TARGET = udp_server

# Default Arguments
HOST = 127.0.0.1
PORT = 3081

# Default rule
all: $(CLIENT_TARGET) $(SERVER_TARGET)

# Build and run instantly
client: $(CLIENT_TARGET)
	./$(CLIENT_TARGET) $(HOST) $(PORT)

server: $(SERVER_TARGET)
	./$(SERVER_TARGET) $(PORT)

# Link the executable
$(CLIENT_TARGET): $(OBJS) $(CLIENT_OBJ)
	$(CXX) $(OBJS) $(CLIENT_OBJ) -o $@ $(LIBS)

$(SERVER_TARGET): $(OBJS) $(SERVER_OBJ)
	$(CXX) $(OBJS) $(SERVER_OBJ) -o $@ $(LIBS)

# Compile source files into object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(SRC_DIR)/%.tpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/udp_client.o: udp_client.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/udp_server.o: udp_server.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create build directory
$(BUILD_DIR):
	rm -rf $(BUILD_DIR)
	mkdir $(BUILD_DIR)

# Clean generated files
clean:
	rm -rf $(BUILD_DIR) $(CLIENT_TARGET) $(SERVER_TARGET)

.PHONY: all clean