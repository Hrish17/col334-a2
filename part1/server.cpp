// server side code for socket connection

#include <iostream>
#include <fstream>
#include <string>
#include <json/json.h>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket == -1) {
        std::cerr << "Failed to create a socket. Exiting..." << std::endl;
        return -1;
    }

    struct sockaddr_in server_address;

    server_address.sin_family = AF_INET;
    
    std::ifstream config_file("config.json");
    Json::Value config;
    config_file >> config;

    int server_ip_address = inet_addr(config["server_ip"].asCString());
    int server_port = config["server_port"].asInt();

    std::cout << "Server IP: " << server_ip_address << std::endl;
    std::cout << "Server Port: " << server_port << std::endl;


    // server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_addr.s_addr = server_ip_address;
    server_address.sin_port = htons(server_port);

    std::cout << "Server IP: " << server_address.sin_addr.s_addr << std::endl;

    // bind the socket to the specified IP and port
    int bind_status = bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address));

    if (bind_status == -1) {
        // print error message and exit
        std::cerr << "Failed to bind the socket to the specified IP and port. Exiting..." << std::endl;
        return -1;
    }

    // listen for incoming connections
    std::cout << "Listening for incoming connections..." << std::endl;

    int listen_status = listen(server_socket, 5);

    while(true) {
        struct sockaddr_in client_address;
        socklen_t client_address_size = sizeof(client_address);

        int client_socket = accept(server_socket, (struct sockaddr *) &client_address, &client_address_size);

        if (client_socket == -1) {
            std::cerr << "Failed to accept the incoming connection. Exiting..." << std::endl;
            return -1;
        }

        char buffer[1024] = {0};
        int valread = read(client_socket, buffer, 1024);

        std::cout << "Received message from client: " << buffer << std::endl;

        close(client_socket);
    }
    
    close(server_socket);
}