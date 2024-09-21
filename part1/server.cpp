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
#include <vector>

using namespace std;

int main()
{
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket == -1)
    {
        cerr << "Failed to create a socket. Exiting..." << endl;
        return -1;
    }

    struct sockaddr_in server_address;

    server_address.sin_family = AF_INET;

    ifstream config_file("config.json");
    Json::Value config;
    config_file >> config;

    int server_ip_address = inet_addr(config["server_ip"].asCString());
    int server_port = config["server_port"].asInt();

    cout << "Server IP: " << server_ip_address << endl;
    cout << "Server Port: " << server_port << endl;

    // server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_addr.s_addr = server_ip_address;
    server_address.sin_port = htons(server_port);

    cout << "Server IP: " << server_address.sin_addr.s_addr << endl;

    // bind the socket to the specified IP and port
    int bind_status = bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address));

    if (bind_status == -1)
    {
        // print error message and exit
        cerr << "Failed to bind the socket to the specified IP and port. Exiting..." << endl;
        return -1;
    }

    int p = config["p"].asInt(), k = config["k"].asInt();
    if (p > k)
    {
        cerr << "Value of k should be greater than that of p!" << endl;
        return -1;
    }

    string input_file = config["input_file"].asString();

    // listen for incoming connections
    cout << "Listening for incoming connections..." << endl;

    int listen_status = listen(server_socket, 5);

    while (true)
    {
        struct sockaddr_in client_address;
        socklen_t client_address_size = sizeof(client_address);

        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_size);

        if (client_socket == -1)
        {
            cerr << "Failed to accept the incoming connection. Exiting..." << endl;
            return -1;
        }

        // if a connection is accepted, print the client IP and port
        cout << "Connection accepted from " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << endl;

        // start receiving data from the client

        ifstream file(input_file);
        // comma separated words
        vector<string> words;
        words.push_back("dummy");
        string word;
        while (getline(file, word, ','))
        {
            words.push_back(word);
        }

        while (true)
        {
            int offset;

            // receive the offset from the client
            int recv_status = recv(client_socket, &offset, sizeof(offset), 0);

            if (recv_status <= 0)
            {
                // if recv fails or client closes the connection, exit the loop
                cerr << "Client disconnected or recv failed. Exiting..." << endl;
                break;
            }

            // if client sends -1 as offset, gracefully stop the server side for this client
            if (offset == -1)
            {
                cout << "Received termination signal from client. Closing connection..." << endl;
                break;
            }

            // Check if the offset is valid
            if (offset >= words.size())
            {
                // send special packet indicating invalid offset
                string result = "$$\n";
                cout << "Sending data: " << result << endl;
                send(client_socket, result.c_str(), result.size(), 0);
                continue; // continue to the next iteration waiting for valid requests
            }

            // send the result to the client
            int words_sent = 0;
            while (words_sent < k)
            {
                string result = "";
                bool eof = false;

                // send p words in a packet
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
                // print the result to the console
                cout << "Sending data: " << result << endl;
                send(client_socket, result.c_str(), result.size(), 0);
                words_sent += p;

                if (eof)
                {
                    break; // stop sending once we reach the EOF
                }
            }
        }
    }

    close(server_socket);
}