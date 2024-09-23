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
#include <thread>
#include <map>

using namespace std;

void run_client(int client_id, const string &server_ip, int server_port, int k, int p) {
    // Create a TCP socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        cerr << "Client " << client_id << ": Failed to create a socket. Exiting..." << endl;
        return;
    }

    // Specify the address and port of the server
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(server_ip.c_str());
    server_address.sin_port = htons(server_port);

    // Connect to the server
    int connection_status = connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address));
    if (connection_status == -1) {
        cerr << "Client " << client_id << ": Failed to connect to the server. Exiting..." << endl;
        close(client_socket);
        return;
    }
    cout << "Client " << client_id << ": Connected to the server." << endl;

    int offset = 0;
    map<string, int> word_count;
    string leftover_data = "";

    while (true) {
        // Send the offset to the server
        send(client_socket, &offset, sizeof(offset), 0);
        cout << "Client " << client_id << ": Sent offset: " << offset << endl;

        int words_received = 0;
        bool eof_received = false;

        while (words_received < k) {
            char buffer[1024] = {0};
            int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0) {
                cerr << "Client " << client_id << ": Failed to receive data from the server or server closed connection." << endl;
                eof_received = true;
                break;
            }

            string data = leftover_data + string(buffer, bytes_received);
            leftover_data = "";

            size_t pos = 0;
            while ((pos = data.find('\n')) != string::npos) {
                string line = data.substr(0, pos);
                data.erase(0, pos + 1);

                if (line == "EOF" || line == "$$") {
                    eof_received = true;
                    break;
                }

                size_t word_pos = 0;
                while ((word_pos = line.find(',')) != string::npos) {
                    string word = line.substr(0, word_pos);
                    if (!word.empty()) {
                        words_received++;
                        word_count[word]++;
                    }
                    line.erase(0, word_pos + 1);
                }
            }

            leftover_data = data;

            if (eof_received) {
                break;
            }
        }

        if (eof_received) {
            cout << "Client " << client_id << ": Received EOF. Closing connection." << endl;
            offset = -1;
            send(client_socket, &offset, sizeof(offset), 0);
            break;
        }

        offset += k; // Adjust offset for next batch
    }

    cout << "Client " << client_id << ": Word count:" << endl;
    for (const auto &entry : word_count) {
        cout << entry.first << ": " << entry.second << endl;
    }

    // Close the connection
    close(client_socket);
}

int main() {
    // Read from config file (config.json)
    ifstream config_file("config.json");
    Json::Value config;
    Json::CharReaderBuilder readerBuilder;
    string errors;

    if (!Json::parseFromStream(readerBuilder, config_file, &config, &errors)) {
        cerr << "Failed to parse config.json: " << errors << endl;
        return -1;
    }

    string server_ip = config["server_ip"].asString();
    int server_port = config["server_port"].asInt();
    int k = config["k"].asInt();
    int p = config["p"].asInt();
    int num_clients = config["num_clients"].asInt();

    // Create multiple threads for concurrent clients
    vector<thread> client_threads;
    for (int i = 0; i < num_clients; ++i) {
        client_threads.emplace_back(run_client, i + 1, server_ip, server_port, k, p);
    }

    // Join all threads
    for (auto &t : client_threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    return 0;
}
