all: build

build: client.cpp server.cpp
	g++ -std=c++11 -Wall server.cpp -o server -I/usr/include/jsoncpp -ljsoncpp
	g++ -std=c++11 -Wall client.cpp -o client -I/usr/include/jsoncpp -ljsoncpp	

run:
	./server & \sleep 5
	./client

plot: python.py
	python3 python.py

clean:
	rm -f *.o client server

.PHONY: all build run_server run_client clean plot

kill8080:
	@echo "Killing process on port 8080..."
	@pid=$(shell sudo lsof -t -i :8080) && if [ -n "$$pid" ]; then \
    	echo "Killing process with PID $$pid"; \
    	sudo kill -9 $$pid; \
	else \
    	echo "No process found on port 8080"; \
	fi