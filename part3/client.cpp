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
#include <chrono>
#include <random>

using namespace std;

void run_aloha_client(int client_id, const string &server_ip, int server_port, int k, int p)
{
    // Create a TCP socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1)
    {
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
    if (connection_status == -1)
    {
        cerr << "Client " << client_id << ": Failed to connect to the server. Exiting..." << endl;
        close(client_socket);
        return;
    }
    cout << "Client " << client_id << ": Connected to the server." << endl;

    int offset = 0;
    map<string, int> word_count;
    string leftover_data = "";

    while (true)
    {
        // Send the offset to the server
        send(client_socket, &offset, sizeof(offset), 0);
        cout << "Client " << client_id << ": Sent offset: " << offset << endl;

        int words_received = 0;
        bool eof_received = false;

        while (words_received < k)
        {
            char buffer[1024] = {0};
            int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0)
            {
                cerr << "Client " << client_id << ": Failed to receive data from the server or server closed connection." << endl;
                eof_received = true;
                break;
            }

            string data = leftover_data + string(buffer, bytes_received);
            leftover_data = "";

            size_t pos = 0;
            while ((pos = data.find('\n')) != string::npos)
            {
                string line = data.substr(0, pos);
                data.erase(0, pos + 1);

                if (line == "EOF" || line == "$$")
                {
                    eof_received = true;
                    break;
                }

                size_t word_pos = 0;
                while ((word_pos = line.find(',')) != string::npos)
                {
                    cout << "Client " << client_id << ": Received word: " << line.substr(0, word_pos) << endl;
                    string word = line.substr(0, word_pos);
                    if (!word.empty())
                    {
                        words_received++;
                        word_count[word]++;
                    }
                    line.erase(0, word_pos + 1);
                }
            }

            leftover_data = data;

            if (eof_received)
            {
                break;
            }
        }

        if (eof_received)
        {
            cout << "Client " << client_id << ": Received EOF. Closing connection." << endl;
            offset = -1;
            send(client_socket, &offset, sizeof(offset), 0);
            break;
        }

        offset += k;
    }

    cout << "Client " << client_id << ": Word count:" << endl;
    for (const auto &entry : word_count)
    {
        cout << entry.first << ": " << entry.second << endl;
    }

    close(client_socket);
}

void run_beb_client(int client_id, const string &server_ip, int server_port, int k, int p)
{
    // Create a TCP socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1)
    {
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
    if (connection_status == -1)
    {
        cerr << "Client " << client_id << ": Failed to connect to the server. Exiting..." << endl;
        close(client_socket);
        return;
    }
    cout << "Client " << client_id << ": Connected to the server." << endl;

    int offset = 0;
    map<string, int> word_count;
    string leftover_data = "";

    // Define the backoff parameters
    const int MAX_ATTEMPTS = 10;
    const int SLOT_DURATION = 100; // Example slot duration in milliseconds
    random_device rd;
    mt19937 generator(rd());
    uniform_int_distribution<> distribution(0, 1);

    while (true)
    {
        // Send the offset to the server
        send(client_socket, &offset, sizeof(offset), 0);
        cout << "Client " << client_id << ": Sent offset: " << offset << endl;

        int words_received = 0;
        bool eof_received = false;

        while (words_received < k)
        {
            char buffer[1024] = {0};
            int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0)
            {
                cerr << "Client " << client_id << ": Failed to receive data from the server or server closed connection." << endl;
                eof_received = true;
                break;
            }

            string data = leftover_data + string(buffer, bytes_received);
            leftover_data = "";

            size_t pos = 0;
            while ((pos = data.find('\n')) != string::npos)
            {
                cout << "Client " << client_id << ": Received data: " << data.substr(0, pos) << endl;
                string line = data.substr(0, pos);
                data.erase(0, pos + 1);

                if (line == "EOF" || line == "$$")
                {
                    eof_received = true;
                    break;
                }

                size_t word_pos = 0;
                while ((word_pos = line.find(',')) != string::npos)
                {
                    string word = line.substr(0, word_pos);
                    if (!word.empty())
                    {
                        words_received++;
                        word_count[word]++;
                    }
                    line.erase(0, word_pos + 1);
                }
            }

            leftover_data = data;

            if (eof_received)
            {
                break;
            }
        }

        if (eof_received)
        {
            cout << "Client " << client_id << ": Received EOF. Closing connection." << endl;
            offset = -1;
            send(client_socket, &offset, sizeof(offset), 0);
            break;
        }

        offset += k;

        // Simulate backoff with random delay
        int attempts = 0;
        while (attempts < MAX_ATTEMPTS)
        {
            this_thread::sleep_for(chrono::milliseconds(SLOT_DURATION * (1 << attempts)));
            attempts++;
            if (distribution(generator) == 1)
            {
                break; // Retry
            }
        }
    }

    cout << "Client " << client_id << ": Word count:" << endl;
    for (const auto &entry : word_count)
    {
        cout << entry.first << ": " << entry.second << endl;
    }

    close(client_socket);
}

void run_sensing_client(int client_id, const string &server_ip, int server_port, int k, int p)
{
    // Create a TCP socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1)
    {
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
    if (connection_status == -1)
    {
        cerr << "Client " << client_id << ": Failed to connect to the server. Exiting..." << endl;
        close(client_socket);
        return;
    }
    cout << "Client " << client_id << ": Connected to the server." << endl;

    int offset = 0;
    map<string, int> word_count;
    string leftover_data = "";

    // Define control message handling
    const int POLL_INTERVAL = 100; // Polling interval in milliseconds

    while (true)
    {
        // Send control message to check if server is busy
        send(client_socket, "BUSY?\n", 6, 0);
        char response[1024] = {0};
        int bytes_received = recv(client_socket, response, sizeof(response) - 1, 0);
        if (bytes_received <= 0)
        {
            cerr << "Client " << client_id << ": Failed to receive response from the server." << endl;
            break;
        }

        string server_response(response);
        if (server_response.find("IDLE") != string::npos)
        {
            // Send the offset to the server
            send(client_socket, &offset, sizeof(offset), 0);
            cout << "Client " << client_id << ": Sent offset: " << offset << endl;

            int words_received = 0;
            bool eof_received = false;

            while (words_received < k)
            {
                char buffer[1024] = {0};
                int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
                if (bytes_received <= 0)
                {
                    cerr << "Client " << client_id << ": Failed to receive data from the server or server closed connection." << endl;
                    eof_received = true;
                    break;
                }

                string data = leftover_data + string(buffer, bytes_received);
                leftover_data = "";

                size_t pos = 0;
                while ((pos = data.find('\n')) != string::npos)
                {
                    cout << "Client " << client_id << ": Received data: " << data.substr(0, pos) << endl;
                    string line = data.substr(0, pos);
                    data.erase(0, pos + 1);

                    if (line == "EOF" || line == "$$")
                    {
                        eof_received = true;
                        break;
                    }

                    size_t word_pos = 0;
                    while ((word_pos = line.find(',')) != string::npos)
                    {
                        string word = line.substr(0, word_pos);
                        if (!word.empty())
                        {
                            words_received++;
                            word_count[word]++;
                        }
                        line.erase(0, word_pos + 1);
                    }
                }

                leftover_data = data;

                if (eof_received)
                {
                    break;
                }
            }

            if (eof_received)
            {
                cout << "Client " << client_id << ": Received EOF. Closing connection." << endl;
                offset = -1;
                send(client_socket, &offset, sizeof(offset), 0);
                break;
            }

            offset += k;
        }
        else
        {
            this_thread::sleep_for(chrono::milliseconds(POLL_INTERVAL));
        }
    }

    cout << "Client " << client_id << ": Word count:" << endl;
    for (const auto &entry : word_count)
    {
        cout << entry.first << ": " << entry.second << endl;
    }

    close(client_socket);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cerr << "Usage: " << argv[0] << " <protocol>" << endl;
        return 1;
    }

    string protocol = argv[1];

    // Read configuration from config.json
    ifstream config_file("config.json");
    if (!config_file.is_open())
    {
        cerr << "Error opening config.json file." << endl;
        return 1;
    }

    Json::Value config;
    config_file >> config;
    config_file.close();

    string server_ip = config["server_ip"].asString();
    int server_port = config["server_port"].asInt();
    int k = config["k"].asInt();
    int p = config["p"].asInt();

    vector<thread> client_threads;

    for (int i = 0; i < 5; ++i)
    {
        if (protocol == "aloha")
        {
            client_threads.emplace_back(run_aloha_client, i, server_ip, server_port, k, p);
        }
        else if (protocol == "beb")
        {
            client_threads.emplace_back(run_beb_client, i, server_ip, server_port, k, p);
        }
        else if (protocol == "sensing")
        {
            client_threads.emplace_back(run_sensing_client, i, server_ip, server_port, k, p);
        }
        else
        {
            cerr << "Unknown protocol: " << protocol << endl;
            return 1;
        }
    }

    for (auto &t : client_threads)
    {
        t.join();
    }

    return 0;
}
