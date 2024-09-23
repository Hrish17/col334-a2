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
#include <random>

using namespace std;

mutex server_mutex;
bool server_busy = false;
int busy_socket = -1;
chrono::time_point<chrono::steady_clock> last_collision_time;

void handle_client(int client_socket, vector<string> words, int k, int p, string protocol)
{
    while (true)
    {
        int offset;
        // Receive the offset from the client
        int recv_status = recv(client_socket, &offset, sizeof(offset), 0);
        if (recv_status <= 0)
        {
            cerr << "Client disconnected or recv failed. Exiting..." << endl;
            break;
        }

        // If client sends -1 as offset, gracefully stop the server side for this client
        if (offset == -1)
        {
            cout << "Received termination signal from client. Closing connection..." << endl;
            break;
        }

        // Protocol behavior: Check for concurrency (Grumpy Server behavior)
        lock_guard<mutex> lock(server_mutex);
        if (server_busy && busy_socket != client_socket)
        {
            if (protocol == "aloha" || protocol == "beb")
            {
                string huh_msg = "HUH!\n";
                send(client_socket, huh_msg.c_str(), huh_msg.size(), 0);
                cout << "Sent HUH! to client " << client_socket << endl;
                continue;
            }
            if (protocol == "sensing")
            {
                string busy_msg = "BUSY\n";
                send(client_socket, busy_msg.c_str(), busy_msg.size(), 0);
                cout << "Sent BUSY to client " << client_socket << endl;
                continue;
            }
        }

        // Mark the server as busy
        server_busy = true;
        busy_socket = client_socket;

        // Check if the offset is valid
        if (offset >= words.size())
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
            cout << "Words sent : " << words_sent << endl;

            // Send p words in a packet
            for (int i = 0; i < p; i++)
            {
                if (offset + words_sent + i >= words.size())
                {
                    eof = true;
                    break;
                }
                result += words[offset + words_sent + i] + ",";
            }

            if (eof)
            {
                result += "EOF,";
            }
            result += "\n";
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
    if (argc < 2)
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

    string input_file = config["input_file"].asString();
    ifstream file(input_file);
    vector<string> words;
    string word;
    while (getline(file, word, ','))
    {
        words.push_back(word);
    }

    cout << "Listening for incoming connections with protocol: " << protocol << "..." << endl;
    int listen_status = listen(server_socket, 5);

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