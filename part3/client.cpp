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

void run_aloha_client(int client_id, const string &server_ip, int server_port, int k, int p, int n, int T)
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

    int offset = 1;
    map<string, int> word_count;
    string leftover_data = "";

    double prob = 1.0 / (double)n;

    while (true)
    {
        // Send the offset to the server
        string offset_str = to_string(offset) + "\n"; // Convert integer to string with newline
        // select a random number between 0 and 1
        double random_number = (double)rand() / RAND_MAX;
        // if current time (in milliseconds) is not a multiple of T, then continue
        int64_t current_time = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
        if (random_number > prob || (current_time % T) != 0)
        {
            continue;
        }
        int send_status = send(client_socket, offset_str.c_str(), offset_str.size(), 0);
        if (send_status == -1)
        {
            cerr << "Failed to send offset to the server." << endl;
        }

        cout << "Client " << client_id << ": Sent offset: " << offset_str << endl;

        int words_received = 0;
        bool eof_received = false;

        vector<string> words;
        bool huh_received = false;
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

            // check if the server has send "HUH!\n" message
            if (strcmp(buffer, "HUH!\n") == 0)
            {
                cout << "Client " << client_id << ": Received HUH! message. Resending offset." << endl;
                huh_received = true;
                break;
            }

            string data = leftover_data + string(buffer, bytes_received);
            leftover_data = "";

            size_t pos = 0;
            while ((pos = data.find('\n')) != string::npos)
            {
                string word = data.substr(0, pos);
                data.erase(0, pos + 1);

                if (word == "EOF" || word == "$$")
                {
                    eof_received = true;
                    break;
                }

                // size_t word_pos = 0;
                // while ((word_pos = line.find(',')) != string::npos)
                // {

                // }
                cout << "Client " << client_id << ": Received word: " << word << endl;
                // string word = line.substr(0, word_pos);
                if (!word.empty())
                {
                    words_received++;
                    words.push_back(word);
                }
                // line.erase(0, pos + 1);
            }

            leftover_data = data;

            if (eof_received)
            {
                break;
            }
        }

        if (!huh_received)
        {
            for (const auto &word : words)
            {
                word_count[word]++;
            }
        }
        else
        {
            continue;
        }

        if (eof_received)
        {
            cout << "Client " << client_id << ": Received EOF. Closing connection." << endl;
            offset_str = "-1\n";
            send(client_socket, offset_str.c_str(), offset_str.size(), 0);
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

void run_beb_client(int client_id, const string &server_ip, int server_port, int k, int p, int T)
{
    // Create a socket for the client
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1)
    {
        cerr << "Client " << client_id << ": Failed to create socket. Exiting..." << endl;
        return;
    }

    // Server address
    sockaddr_in server_address;
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

    int offset = 1;
    int time_to_send = 0;
    int attempt = 1;
    int last_collision_time = 0;
    map<string, int> word_count;
    string leftover_data = "";

    while (true)
    {
        string offset_str = to_string(offset) + "\n";
        // current time in milliseconds
        int64_t current_time = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
        if (current_time % T != 0 || (current_time - last_collision_time) < time_to_send)
        {
            continue;
        }

        int send_status = send(client_socket, offset_str.c_str(), offset_str.size(), 0);
        if (send_status == -1) {
            cerr << "Failed to send offset to the server." << endl;
        }
        cout << "Client " << client_id << ": Sent offset: " << offset << endl;

        int words_received = 0;
        bool huh_received = false;
        bool eof_received = false;

        vector<string> words;

        while (words_received < k)
        {
            char buffer[1024] = {0};
            int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0)
            {
                cerr << "Client " << client_id << ": Failed to receive data from the server." << endl;
                eof_received = true;
                break;
            }

            if (strcmp(buffer, "HUH!\n") == 0)
            {
                cout << "Client " << client_id << ": Received HUH! message. Resending offset." << endl;
                huh_received = true;
                attempt++;
                // randomly choose a number [0, 2^(attempt) - 1]
                int rand_num = rand() % (1 << attempt);
                last_collision_time = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
                time_to_send = rand_num * T;
                break;
            }

            string data = leftover_data + string(buffer, bytes_received);
            leftover_data = "";

            size_t pos = 0;
            while ((pos = data.find('\n')) != string::npos)
            {
                cout << "Client " << client_id << ": Received data: " << data.substr(0, pos) << endl;
                string word = data.substr(0, pos);
                data.erase(0, pos + 1);

                if (word == "EOF" || word == "$$")
                {
                    eof_received = true;
                    break;
                }

                if (!word.empty())
                {
                    words_received++;
                    words.push_back(word);
                }
            }

            leftover_data = data;

            if (eof_received)
            {
                break;
            }
        }

        if (!huh_received)
        {
            for (const auto &word : words)
            {
                word_count[word]++;
            }
            attempt = 1;
            time_to_send = 0;
        }
        else
        {
            continue;
        }

        if (eof_received)
        {
            cout << "Client " << client_id << ": Received EOF. Closing connection." << endl;
            offset_str = "-1\n";
            send(client_socket, offset_str.c_str(), offset_str.size(), 0);
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

void run_sensing_client(int client_id, const string &server_ip, int server_port, int k, int p, int T)
{
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1)
    {
        cerr << "Client " << client_id << ": Failed to create socket. Exiting..." << endl;
        return;
    }

    // Server address
    sockaddr_in server_address;
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

    int offset = 1;
    int time_to_send = 0;
    int attempt = 1;
    int last_collision_time = 0;
    map<string, int> word_count;
    string leftover_data = "";
    bool is_idle = false;

    while (true)
    {
        if(!is_idle){
            string msg = "BUSY?\n";
            send(client_socket, msg.c_str(), msg.size(), 0);
            char buffer[1024] = {0};
            int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0)
            {
                cerr << "Client " << client_id << ": Failed to receive data from the server." << endl;
                break;
            }
            if (strcmp(buffer, "BUSY!\n") == 0)
            {
                cout << "Client " << client_id << ": Server is busy. Waiting..." << endl;
                continue;
            }
            else if (strcmp(buffer, "IDLE!\n") == 0)
            {
                cout << "Client " << client_id << ": Server is idle. Sending offset." << endl;
                is_idle = true;
            }
            cout << "Client " << client_id << ": Received data: " << buffer << endl;
            continue;
        }
        string offset_str = to_string(offset) + "\n";
        // current time in milliseconds
        int64_t current_time = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
        if (current_time % T != 0 || (current_time - last_collision_time) < time_to_send)
        {
            continue;
        }

        int send_status = send(client_socket, offset_str.c_str(), offset_str.size(), 0);
        if (send_status == -1) {
            cerr << "Failed to send offset to the server." << endl;
        }
        cout << "Client " << client_id << ": Sent offset: " << offset << endl;

        int words_received = 0;
        bool huh_received = false;
        bool eof_received = false;

        vector<string> words;

        while (words_received < k)
        {
            char buffer[1024] = {0};
            int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0)
            {
                cerr << "Client " << client_id << ": Failed to receive data from the server." << endl;
                eof_received = true;
                break;
            }

            if (strcmp(buffer, "HUH!\n") == 0)
            {
                cout << "Client " << client_id << ": Received HUH! message. Resending offset." << endl;
                huh_received = true;
                attempt++;
                last_collision_time = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
                // randomly choose a number [0, 2^(attempt) - 1]
                int rand_num = rand() % (1 << attempt);
                time_to_send = rand_num * T;
                break;
            }

            string data = leftover_data + string(buffer, bytes_received);
            leftover_data = "";

            size_t pos = 0;
            while ((pos = data.find('\n')) != string::npos)
            {
                cout << "Client " << client_id << ": Received data: " << data.substr(0, pos) << endl;
                string word = data.substr(0, pos);
                data.erase(0, pos + 1);

                if (word == "EOF" || word == "$$")
                {
                    eof_received = true;
                    break;
                }

                if (!word.empty())
                {
                    words_received++;
                    words.push_back(word);
                }
            }

            leftover_data = data;

            if (eof_received)
            {
                break;
            }
        }

        if (!huh_received)
        {
            for (const auto &word : words)
            {
                word_count[word]++;
            }
            attempt = 1;
            time_to_send = 0;
        }
        else
        {
            continue;
        }

        if (eof_received)
        {
            cout << "Client " << client_id << ": Received EOF. Closing connection." << endl;
            offset_str = "-1\n";
            send(client_socket, offset_str.c_str(), offset_str.size(), 0);
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
    int T = config["T"].asInt();

    int num_clients = config["num_clients"].asInt();

    vector<thread> client_threads;

    for (int i = 0; i < num_clients; ++i)
    {
        if (protocol == "aloha")
        {
            client_threads.emplace_back(run_aloha_client, i, server_ip, server_port, k, p, num_clients, T);
        }
        else if (protocol == "beb")
        {
            client_threads.emplace_back(run_beb_client, i, server_ip, server_port, k, p, T);
        }
        else if (protocol == "sensing")
        {
            client_threads.emplace_back(run_sensing_client, i, server_ip, server_port, k, p, T);
        }
        else
        {
            cerr << "Unknown protocol: " << protocol << endl;
            return 1;
        }
    }

    for (auto &t : client_threads)
    {
        if (t.joinable())
        {
            t.join();
        }
    }

    return 0;
}
