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
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

using namespace std;

struct Request {
    int client_socket;
    int offset;
};

// Shared queue, mutex, and condition variable
queue<Request> request_queue;
mutex queue_mutex;
vector<int> client_sockets;
vector<Request> request_list;
mutex request_list_mutex;
condition_variable cv;
bool server_running = true; // Flag to indicate server status

// Function to handle client requests (push them to the queue)
void handle_client(int client_socket) {
    while (true) {
        int offset;
        // Receive the offset from the client
        int recv_status = recv(client_socket, &offset, sizeof(offset), 0);
        if (recv_status <= 0) {
            cerr << "Client disconnected or recv failed. Exiting..." << endl;
            break;
        }

        cout << "Received offset: " << offset << endl;

        // If client sends -1 as offset, gracefully stop the server side for this client
        if (offset == -1) {
            cout << "Received termination signal from client. Closing connection..." << endl;
            break;
        }

        // Push the request into the queue
        {
            lock_guard<mutex> lock(queue_mutex);
            request_queue.push({client_socket, offset});
            // lock_guard<mutex> lock(request_list_mutex);
            request_list.push_back({client_socket, offset});
        }
        cv.notify_one(); // Notify the controller thread that a new request is available
    }
    close(client_socket);
}

// Function to process requests from the queue in FIFO manner
void fifo_controller_thread_function(vector<string> words, int p, int k) {
    while (server_running) {
        Request req;

        // Wait for a request to be available in the queue
        {
            unique_lock<mutex> lock(queue_mutex);
            cv.wait(lock, [] { return !request_queue.empty() || !server_running; });

            // Exit if the server is shutting down
            if (!server_running && request_queue.empty()) {
                return; // Exit thread if no requests and server is not running
            }

            // Pop the request from the queue
            if (!request_queue.empty()) {
                req = request_queue.front();
                request_queue.pop();
            }
        }

        if (req.client_socket) { // Process the request only if there is a valid socket
            int client_socket = req.client_socket;
            int offset = req.offset;

            // Process the request
            if (offset >= words.size()) {
                // Send special packet indicating invalid offset
                string result = "$$\n";
                cout << "Sending data: " << result << endl;
                send(client_socket, result.c_str(), result.size(), 0);
                continue;
            }

            // Send the result to the client
            int words_sent = 0;
            while (words_sent < k) {
                string result = "";
                bool eof = false;

                // Send p words in a packet
                for (int i = 0; i < p; i++) {
                    if (offset + words_sent + i >= words.size()) {
                        eof = true;
                        break;
                    }
                    result += words[offset + words_sent + i] + "\n";
                }

                if (eof) {
                    result += "EOF\n";
                }
                cout << "Sending data: " << result << endl;
                send(client_socket, result.c_str(), result.size(), 0);
                words_sent += p;

                if (eof) {
                    break;
                }
            }
        }
    }
}


// mutex request_list_mutex;

// Function to process requests from the queue in Round-Robin manner
void rr_controller_thread_function(vector<string> words, int p, int k) {

    int current_client_index = 0;

    while (server_running) {
        if (client_sockets.empty()) {
            this_thread::sleep_for(chrono::milliseconds(100)); // Sleep if no clients
            continue;
        }

        int client_socket = client_sockets[current_client_index];

        int offset;
        // find the offset for client in the request list
        for (int i = 0; i < request_list.size(); i++) {
            if (request_list[i].client_socket == client_socket) {
                offset = request_list[i].offset;
                request_list.erase(request_list.begin() + i);
                break;
            }
        }
        

        // Process the request
        if (offset >= words.size()) {
            // Send special packet indicating invalid offset
            string result = "$$\n";
            cout << "Sending data: " << result << endl;
            send(client_socket, result.c_str(), result.size(), 0);
            continue;
        }

        // Send the result to the client
        int words_sent = 0;
        while (words_sent < k) {
            string result = "";
            bool eof = false;

            // Send p words in a packet
            for (int i = 0; i < p; i++) {
                if (offset + words_sent + i >= words.size()) {
                    eof = true;
                    break;
                }
                result += words[offset + words_sent + i] + "\n";
            }

            if (eof) {
                result += "EOF\n";
            }
            cout << "Sending data: " << result << endl;
            send(client_socket, result.c_str(), result.size(), 0);
            words_sent += p;

            if (eof) {
                break;
            }
        }

        // Move to the next client in a round-robin fashion
        current_client_index = (current_client_index + 1) % client_sockets.size();
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <mode (fifo | rr)>" << endl;
        return -1;
    }

    bool is_fifo = (strcmp(argv[1], "fifo") == 0);
    bool is_rr = (strcmp(argv[1], "rr") == 0);

    if (!is_fifo && !is_rr) {
        cerr << "Invalid mode specified. Use 'fifo' or 'rr'." << endl;
        return -1;
    }

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        cerr << "Failed to create a socket. Exiting..." << endl;
        return -1;
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;

    cout << "Reading configuration from config.json..." << endl;

    ifstream config_file("config.json");
    Json::Value config;
    config_file >> config;

    int server_ip_address = inet_addr(config["server_ip"].asCString());
    int server_port = config["server_port"].asInt();
    int num_clients = config["num_clients"].asInt();

    cout << "Server IP: " << server_ip_address << endl;
    cout << "Server Port: " << server_port << endl;

    server_address.sin_addr.s_addr = server_ip_address;
    server_address.sin_port = htons(server_port);

    // Bind the socket to the specified IP and port
    int bind_status = bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address));
    if (bind_status == -1) {
        cerr << "Failed to bind the socket to the specified IP and port. Exiting..." << endl;
        return -1;
    }

    int p = config["p"].asInt(), k = config["k"].asInt();
    if (p > k) {
        cerr << "Value of k should be greater than that of p!" << endl;
        return -1;
    }

    string input_file = config["input_file"].asString();

    // Load words from input file into a vector
    ifstream file(input_file);
    vector<string> words;
    words.push_back("dummy");  // First entry is a dummy
    string word;
    while (getline(file, word, ',')) {
        words.push_back(word);
    }

    // Listen for incoming connections
    cout << "Listening for incoming connections..." << endl;
    int listen_status = listen(server_socket, num_clients);

    // Start the controller thread based on the mode
    thread controller_thread;
    if (is_fifo) {
        controller_thread = thread(fifo_controller_thread_function, words, p, k);
    } else {
        controller_thread = thread(rr_controller_thread_function, words, p, k);
    }

    vector<thread> client_threads;
    for (int i = 0; i < num_clients; i++) {
        struct sockaddr_in client_address;
        socklen_t client_address_size = sizeof(client_address);

        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_size);
        if (client_socket == -1) {
            cerr << "Failed to accept the incoming connection. Exiting..." << endl;
            continue;
        }

        cout << "Connection accepted from " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << endl;

        // Create a new thread for the client
        client_threads.push_back(thread(handle_client, client_socket));
        client_sockets.push_back(client_socket);
    }

    // Wait for all client threads to finish
    for (auto &t : client_threads) {
        t.join();
    }

    // Signal the controller to stop
    {
        lock_guard<mutex> lock(queue_mutex);
        server_running = false;
    }
    cv.notify_all(); // Notify the controller thread to exit

    // Wait for the controller thread to finish
    controller_thread.join();

    // Close the server socket
    close(server_socket);
    cout << "Server shut down successfully." << endl;

    return 0;
}
