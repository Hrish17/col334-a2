# Define variables
CXX = g++
CXXFLAGS = -std=c++11 -Wall
CLIENT_SRCS = client.cpp
SERVER_SRCS = server.cpp
CLIENT_EXE = client
SERVER_EXE = server
PLOT_SCRIPT = plot.py
CONFIG_FILE = config.json
JSON_FLAG = -I/usr/include/jsoncpp -ljsoncpp

# Default target
all: build

# Build the client and server
build: client server

client: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) $(CLIENT_SRCS) -o $(CLIENT_EXE) $(JSON_FLAG)

server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) $(SERVER_SRCS) -o $(SERVER_EXE) $(JSON_FLAG)

# Run the client and server
run:
	chmod +x run_server_client.sh
	./run_server_client.sh

# Clean the build
clean:
	rm -f *.o client server

# Run the experiment and plot the results
run_experiment: clean build
	# Run server in the background
	./server & SERVER_PID=$$!; \
	# sleep 1; \
	# # Run client multiple times and log results
	# for p in 1 2 3 4 5 6 7 8 9 10; do \
	# 	echo "Running experiment with p = $$p"; \
	# 	./client; \
	# done; \
	# Kill the server after running the experiments
	# kill $$SERVER_PID

plot: $(PLOT_SCRIPT)
	python3 $(PLOT_SCRIPT)

.PHONY: all build run_server run_client clean run_experiment plot
