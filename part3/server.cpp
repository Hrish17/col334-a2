#include <iostream>
#include <fstream>
#include <json/json.h>
#include <string>
#include <cstring>
#include <vector>
#include <thread>
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>

using namespace std;

mutex server_mutex;
bool server_busy = false;
int busy_socket = -1;
chrono::time_point<chrono::steady_clock> last_collision_time;

void handle_client(int client_socket, vector<string> words, int k, int p, string protocol)
{
    while (true)
    {
        char buffer[1024];                                                    // Create a buffer large enough to hold incoming data
        int recv_status = recv(client_socket, buffer, sizeof(buffer) - 1, 0); // Leave space for null-terminator
        int offset;
        string message;
        if (recv_status > 0)
        {
            buffer[recv_status] = '\0'; // Null-terminate the received string
            string received_str(buffer);

            // Remove the newline character if present
            if (received_str.back() == '\n')
            {
                received_str.pop_back();
            }

            // Convert the string back to an integer
            if (received_str != "BUSY?")
            {
                cout << "Received data: " << received_str << " from client " << client_socket << endl;
                cout << "Received offset: " << received_str << endl;
                offset = stoi(received_str);
            }
            else
            {
                message = received_str;
                offset = -1;
            }

            cout << "Received offset: " << offset << endl;
        }
        else if (recv_status == 0)
        {
            cerr << "Client disconnected." << endl;
        }
        else
        {
            cerr << "Error receiving data: " << strerror(errno) << endl;
        }
        // Store the arrival time of the message
        auto arrival_time = chrono::steady_clock::now();

        // Parse the offset (assuming the client sends an integer as a string)

        // Protocol behavior: Check for concurrency (Grumpy Server behavior)
        lock_guard<mutex> lock(server_mutex);
        if ((server_busy && busy_socket != client_socket) || (arrival_time <= last_collision_time))
        {
            last_collision_time = chrono::steady_clock::now();
            if (protocol == "aloha" || protocol == "beb" || (protocol == "sensing" && message != "BUSY?\n"))
            {
                string huh_msg = "HUH!\n";
                send(client_socket, huh_msg.c_str(), huh_msg.size(), 0);
                cout << "Sent HUH! to client " << client_socket << endl;
                continue;
            }
            if (protocol == "sensing" && message == "BUSY?\n")
            {
                string busy_msg = "BUSY\n";
                send(client_socket, busy_msg.c_str(), busy_msg.size(), 0);
                cout << "Sent BUSY to client " << client_socket << endl;
                continue;
            }
        }
        else if (!server_busy && protocol == "sensing" && message == "BUSY?/n"){
            string busy_msg = "IDLE/n";
            send(client_socket, busy_msg.c_str(), busy_msg.size(), 0);
            cout << "Sent IDLE to client " << client_socket << endl;
        }

        // Mark the server as busy
        server_busy = true;
        busy_socket = client_socket;

        // Check if the offset is valid
        if (offset < 0 || offset >= words.size())
        {
            string result = "$$\n";
            send(client_socket, result.c_str(), result.size(), 0);
            continue;
        }

        // Send words to the client
        int words_sent = 0;
        while (words_sent < k)
        {
            string result = "";
            bool eof = false;

            // Send p words in a packet
            for (int i = 0; i < p; i++)
            {
                if (offset + words_sent + i >= words.size())
                {
                    eof = true;
                    break;
                }
                result += words[offset + words_sent + i] + "\n";
            }

            if (eof)
            {
                result += "EOF\n";
            }
            // result += "\n";
            cout << "Sending data: " << result << " to client " << client_socket << endl;
            send(client_socket, result.c_str(), result.size(), 0);
            words_sent += p;

            if (eof)
            {
                break;
            }
        }

        // Mark the server as idle
        server_busy = false;
        busy_socket = -1;
    }

    close(client_socket);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cerr << "Usage: ./server <protocol>" << endl;
        return -1;
    }

    string protocol = argv[1];
    if (protocol != "aloha" && protocol != "beb" && protocol != "sensing")
    {
        cerr << "Invalid protocol! Use 'aloha', 'beb', or 'sensing'." << endl;
        return -1;
    }

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        cerr << "Failed to create a socket. Exiting..." << endl;
        return -1;
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;

    // Reading configuration from config.json
    ifstream config_file("config.json");
    if (!config_file)
    {
        cerr << "Failed to open config.json. Exiting..." << endl;
        return -1;
    }

    Json::Value config;
    config_file >> config;

    int server_ip_address = inet_addr(config["server_ip"].asCString());
    int server_port = config["server_port"].asInt();
    int num_clients = config["num_clients"].asInt();
    int k = config["k"].asInt(), p = config["p"].asInt();

    server_address.sin_addr.s_addr = server_ip_address;
    server_address.sin_port = htons(server_port);

    int bind_status = bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address));
    if (bind_status == -1)
    {
        cerr << "Failed to bind the socket to the specified IP and port. Exiting..." << endl;
        return -1;
    }

    // Read words from the input file
    string input_file = config["input_file"].asString();
    ifstream file(input_file);
    if (!file)
    {
        cerr << "Failed to open input file. Exiting..." << endl;
        return -1;
    }

    vector<string> words;
    string word;
    while (getline(file, word, ','))
    {
        words.push_back(word);
    }

    cout << "Listening for incoming connections with protocol: " << protocol << "..." << endl;
    int listen_status = listen(server_socket, num_clients);
    if (listen_status == -1)
    {
        cerr << "Failed to listen on the socket. Exiting..." << endl;
        return -1;
    }

    vector<thread> client_threads;
    for (int i = 0; i < num_clients; i++)
    {
        struct sockaddr_in client_address;
        socklen_t client_address_size = sizeof(client_address);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_size);
        if (client_socket == -1)
        {
            cerr << "Failed to accept the incoming connection." << endl;
            continue;
        }

        client_threads.push_back(thread(handle_client, client_socket, words, k, p, protocol));
    }

    // Join all client threads
    for (auto &t : client_threads)
    {
        if (t.joinable())
        {
            t.join();
        }
    }

    close(server_socket);
    return 0;
}
