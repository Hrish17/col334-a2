#include <iostream>
#include <fstream>
#include <json/json.h>
#include <string>
#include <cstring>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    // Create a TCP socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket == -1) {
        std::cerr << "Failed to create a socket. Exiting..." << std::endl;
        return -1;
    }

    // Read from config file (config.json)
    std::ifstream config_file("config.json");
    Json::Value config;
    config_file >> config;

    // Extract server IP and port from config
    int server_ip_address = inet_addr(config["server_ip"].asCString());
    int server_port = config["server_port"].asInt();
    
    // Specify the address and port of the server
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = server_ip_address;
    server_address.sin_port = htons(server_port);

    // Connect to the server
    int connection_status = connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address));

    if (connection_status == -1) {
        std::cerr << "Failed to connect to the server. Exiting..." << std::endl;
        return -1;
    } else {
        std::cout << "Connected to the server." << std::endl;
    }

    // Read k (total words) and p (words per packet) from config
    int k = config["k"].asInt();
    int p = config["p"].asInt();

    int offset = 1;

    while (true) {
        // Send the offset to the server
        send(client_socket, &offset, sizeof(offset), 0);

        int words_received = 0;

        // Loop to receive packets from the server until k words are received or "EOF" is encountered
        bool eof_received = false;
        while (words_received < k) {
            char buffer[1024] = {0}; // Adjust size if needed
            int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);

            if (bytes_received == -1) {
                std::cerr << "Failed to receive data from the server." << std::endl;
                break;
            }

            // Convert buffer to string for easy processing
            std::string data(buffer, bytes_received);

            // Check if the server indicated end of data
            if (data.find("EOF") != std::string::npos) {
                std::cout << "Received end of data from server." << std::endl;
                eof_received = true;
                break;
            }

            // Process the received words (assuming comma-separated)
            std::cout << "Received data: " << data << std::endl;
            std::vector<std::string> words;
            size_t pos = 0;
            std::string token;
            while ((pos = data.find(",")) != std::string::npos) {
                token = data.substr(0, pos);
                if (token != "EOF") {
                    words.push_back(token);
                    words_received++;
                }
                data.erase(0, pos + 1);
            }

            if (words_received >= k) {
                std::cout << "Received " << k << " words from the server." << std::endl;
                break;
            }
        }

        std::cout << "Completed receiving words. Enter new offset (-1 to exit): ";
        // std::cin >> offset;
        offset += k;

        // If the client wants to exit, send a special termination signal (-1)
        if (eof_received) {
            offset = -1;
            send(client_socket, &offset, sizeof(offset), 0);
            break;
        }
    }

    // Close the connection
    close(client_socket);
    return 0;
}
