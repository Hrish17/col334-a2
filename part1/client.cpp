// client side code for socket connection
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
#include <map>
#include <chrono>

using namespace std;

int main()
{
    // Create a TCP socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket == -1)
    {
        cerr << "Failed to create a socket. Exiting..." << endl;
        return -1;
    }

    // Read from config file (config.json)
    ifstream config_file("config.json");
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

    if (connection_status == -1)
    {
        // cerr << "Failed to connect to the server. Exiting..." << endl;
        return -1;
    }
    // else
    // {
    //     cout << "Connected to the server." << endl;
    // }

    // Read k (total words) and p (words per packet) from config
    int k = config["k"].asInt();
    int p = config["p"].asInt();

    int offset = 1;
    map<string, int> word_count;

    string leftover_data = ""; // Buffer for leftover data between packets

    chrono::time_point<chrono::steady_clock> start_time = chrono::steady_clock::now();

    while (true)
    {
        // Send the offset to the server
        send(client_socket, &offset, sizeof(offset), 0);

        int words_received = 0;

        // Receive packets until k words are received or "EOF" is encountered
        bool eof_received = false;
        while (words_received < k)
        {
            char buffer[1024] = {0}; // Adjust size if needed
            int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

            if (bytes_received == -1)
            {
                cerr << "Failed to receive data from the server." << endl;
                break;
            }

            // Append received data to leftover data buffer
            string data = leftover_data + string(buffer, bytes_received);
            leftover_data = ""; // Reset leftover data after appending

            // Process complete lines
            size_t pos = 0;
            while ((pos = data.find('\n')) != string::npos)
            {
                string word = data.substr(0, pos);
                data.erase(0, pos + 1); // Erase processed line

                if (word == "EOF" || word == "$$")
                {
                    eof_received = true;
                    break;
                }
                if (!word.empty())
                {
                    words_received++;
                    word_count[word]++;
                }
            }

            // Store any leftover incomplete line for the next receive cycle
            leftover_data = data;

            if (words_received >= k || eof_received)
            {
                break;
            }
        }

        if (eof_received)
        {
            offset = -1;
            send(client_socket, &offset, sizeof(offset), 0);
            break;
        }

        offset += k; // Adjust offset for next batch
    }

    chrono::time_point<chrono::steady_clock> end_time = chrono::steady_clock::now();
    chrono::duration<double> elapsed_time = chrono::duration_cast<chrono::duration<double>>(end_time - start_time);
    ofstream time_file("time.txt", ios::app);
    time_file << elapsed_time.count() << endl;
    time_file.close();

    close(client_socket);
    return 0;
}
