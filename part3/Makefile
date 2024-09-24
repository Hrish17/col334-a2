CXX = g++
CXXFLAGS = -std=c++11 -Wall
CLIENT_SRCS = client.cpp
SERVER_SRCS = server.cpp
CLIENT_EXE = client
SERVER_EXE = server
PLOT_SCRIPT = plot.py
CONFIG_FILE = config.json
JSON_FLAG = -I/usr/include/jsoncpp -ljsoncpp

all: build

build: client server

run-aloha:
	chmod +x run_aloha.sh
	./run_aloha.sh

run-beb:
	chmod +x run_beb.sh
	./run_beb.sh

run-cscd:
	chmod +x run_cscd.sh
	./run_cscd.sh

plot:
	python3 $(PLOT_SCRIPT)

client:
	$(CXX) $(CXXFLAGS) $(CLIENT_SRCS) -o $(CLIENT_EXE) $(JSON_FLAG)

server:
	$(CXX) $(CXXFLAGS) $(SERVER_SRCS) -o $(SERVER_EXE) $(JSON_FLAG)

clean:
	rm -f *.o client server

.PHONY: all build run_server run_client clean plot