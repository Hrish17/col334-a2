// establish a TCP socket connection with the server

#include <iostream>
#include <fstream>
#include <json/json.h>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    // create a tcp socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket == -1) {
        std::cerr << "Failed to create a socket. Exiting..." << std::endl;
        return -1;
    }

    // specify the address and port of the server
    // read from config file (config.json)

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;

    // read from config file
    std::ifstream config_file("config.json");
    Json::Value config;
    config_file >> config;

    int server_ip_address = inet_addr(config["server_ip"].asCString());
    int server_port = config["server_port"].asInt();

    server_address.sin_addr.s_addr = server_ip_address;
    server_address.sin_port = htons(server_port);

    // connect to the server
    int connection_status = connect(client_socket, (struct sockaddr *) &server_address, sizeof(server_address));

    if (connection_status == -1) {
        std::cerr << "Failed to connect to the server. Exiting..." << std::endl;
        return -1;
    } else {
        std::cout << "Connected to the server." << std::endl;
    }
}