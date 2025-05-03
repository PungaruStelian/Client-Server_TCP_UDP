#include "subscriber.h"

std::string recv_string(int sockfd, int len) {
    std::string result(len, '\0'); // Pre-allocate string with the right size
    int bytes_received = recv_all(sockfd, &result[0], len);
    
    if (bytes_received == 0) {
        return ""; // Connection closed
    }
    
    return result;
}

void send_connect_message(int sockfd, const char* id) {
    tcp_request_t connect_packet = {};
    strcpy(connect_packet.id, id);
    connect_packet.type = MESSAGE;
    connect_packet.message = CONNECT;
    send_all(sockfd, &connect_packet, sizeof(connect_packet));
}

void handle_server_message(int sockfd, const char* id, bool& running) {
    // First check message type with peek (without removing from socket buffer)
    int msg_len = 0;
    if (recv(sockfd, &msg_len, sizeof(msg_len), MSG_PEEK) <= 0) {
        running = false;  // Server disconnected or error
        return;
    }
    
    // Determine message type by size - control messages have sizeof(tcp_request_t)
    if (msg_len == sizeof(tcp_request_t)) {
        tcp_request_t control_msg;
        if (recv_all(sockfd, &control_msg, sizeof(control_msg)) <= 0) {
            running = false;  // Error reading full message
            return;
        }
        
        // Special handling for server shutdown notification
        if (control_msg.message == SHUTDOWN) {
            std::cout << "Server is shutting down. Closing connection.\n";
            running = false;
            return;
        }
        
        // Process as data message (non-shutdown control message)
        std::string data((char*)&control_msg, sizeof(control_msg));
        parse_input(data);
    } 
    else {
        // Regular length-prefixed data message
        if (recv_all(sockfd, &msg_len, sizeof(msg_len)) <= 0) {
            running = false;  // Error reading length prefix
            return;
        }
        
        // Receive the actual message content based on length
        std::string data = recv_string(sockfd, msg_len);
        if (data.empty()) {
            running = false;  // Connection closed during receive
            return;
        }
        
        // Process data (extract topic, data type, payload, etc.)
        parse_input(data);
    }
}

bool process_user_command(const char* cmd, int argc, char** argv, int sockfd, const char* id) {
    // Handle exit command - closes the client gracefully
    if (strcmp(cmd, "exit") == 0) {
        if (argc != 1) {
            std::cout << "Usage: exit\n";
            return false;
        }
        
        // Send exit notification to server
        tcp_request_t exit_req = {};
        strcpy(exit_req.id, id);
        exit_req.type = EXIT;
        send_all(sockfd, &exit_req, sizeof(exit_req));
        return true; // Signal to exit client loop
    }
    
    // Handle subscribe command - adds client to a topic (with optional SF flag)
    if (strcmp(cmd, "subscribe") == 0) {
        if (argc < 2) {
            std::cout << "Usage: subscribe <topic> [sf]\n";
            return false;
        }
        
        // Create and send subscription request
        tcp_request_t sub_req = {};
        strcpy(sub_req.id, id);
        sub_req.type = SUBSCRIBE;
        strcpy(sub_req.subscribe.topic, argv[1]);
        sub_req.subscribe.sf = (argc >= 3) ? atoi(argv[2]) : 0;  // Store-and-forward flag
        
        send_all(sockfd, &sub_req, sizeof(sub_req));
        std::cout << "Subscribed to topic.\n";
        return false;
    }
    
    // Handle unsubscribe command - removes client from a topic
    if (strcmp(cmd, "unsubscribe") == 0) {
        if (argc != 2) {
            std::cout << "Usage: unsubscribe <topic>\n";
            return false;
        }
        
        // Create and send unsubscription request
        tcp_request_t unsub_req = {};
        strcpy(unsub_req.id, id);
        unsub_req.type = UNSUBSCRIBE;
        strcpy(unsub_req.subscribe.topic, argv[1]);
        
        send_all(sockfd, &unsub_req, sizeof(unsub_req));
        std::cout << "Unsubscribed from topic.\n";
        return false;
    }
    
    // Command not recognized
    std::cout << "Unknown command: " << cmd << "\n";
    return false;
}

void handle_user_input(int sockfd, const char* id, bool& running) {
    char input_buffer[MESSAGES_SIZE + 1] = {0};
    
    // Read a line of user input
    if (!fgets(input_buffer, sizeof(input_buffer), stdin)) {
        return;  // Error or EOF on stdin
    }
    
    // Split input into command and arguments
    char* args[MESSAGES_SIZE];
    int arg_count = string_to_argv(input_buffer, args);
    
    if (arg_count <= 0) {
        return;  // Empty input or parsing error
    }
    
    // Process the command and update running state if exit requested
    if (process_user_command(args[0], arg_count, args, sockfd, id)) {
        running = false;
    }
}

void subscriber(int sockfd, char* id) {
    // Register with the server first
    send_connect_message(sockfd, id);
    
    // Set up I/O multiplexing to handle both stdin and socket
    fd_set master_set, read_set;
    FD_ZERO(&master_set);
    FD_SET(STDIN_FILENO, &master_set);  // Monitor stdin for user commands
    FD_SET(sockfd, &master_set);        // Monitor socket for server messages
    int max_fd = (sockfd > STDIN_FILENO) ? sockfd : STDIN_FILENO;
    
    // Main event loop
    bool running = true;
    while (running) {
        // Create working copy of the file descriptor set
        read_set = master_set;
        
        // Block until input arrives on one of the descriptors
        int activity = select(max_fd + 1, &read_set, NULL, NULL, NULL);
        if (activity < 0) {
            break;  // Error in select
        }
        
        // Check for and process server messages
        if (FD_ISSET(sockfd, &read_set)) {
            handle_server_message(sockfd, id, running);
        }
        
        // Check for and process user input
        if (FD_ISSET(STDIN_FILENO, &read_set)) {
            handle_user_input(sockfd, id, running);
        }
    }
}

void exit_on_failure(bool condition, const char* message) {
    if (condition) {
        perror(message);
        exit(EXIT_FAILURE);
    }
}

int establish_connection(const char* ip_address, uint16_t port) {
    // Create TCP socket
    int connection_socket = socket(AF_INET, SOCK_STREAM, 0);
    exit_on_failure(connection_socket < 0, "Socket creation error");

    // Configure socket options for better performance
    int option_value = 1;
    setsockopt(connection_socket, SOL_SOCKET, SO_REUSEADDR, &option_value, sizeof(int));
    setsockopt(connection_socket, IPPROTO_TCP, TCP_NODELAY, &option_value, sizeof(int));

    // Prepare server address structure
    sockaddr_in server_info{};
    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(port);
    
    // Convert string IP address to binary form
    int conversion_result = inet_pton(AF_INET, ip_address, &server_info.sin_addr);
    exit_on_failure(conversion_result <= 0, "Address conversion error");

    // Connect to the server
    int connect_result = connect(connection_socket, 
                               (sockaddr*)&server_info, 
                               sizeof(server_info));
    exit_on_failure(connect_result < 0, "Connection failed");

    return connection_socket;
}

int main(int arg_count, char* arg_values[]) {
    // Validate command line arguments
    if (arg_count != 4) {
        fprintf(stderr, "Usage: %s CLIENT_ID SERVER_IP SERVER_PORT\n", arg_values[0]);
        return EXIT_FAILURE;
    }

    // Disable output buffering for immediate feedback
    setvbuf(stdout, nullptr, _IONBF, 0);

    // Parse and validate port number
    char* validation_end;
    long numeric_port = strtol(arg_values[3], &validation_end, 10);
    if (*validation_end != '\0' || numeric_port < 1 || numeric_port > 65535) {
        fprintf(stderr, "Invalid port number\n");
        return EXIT_FAILURE;
    }

    // Connect to server and run client
    int client_socket = establish_connection(arg_values[2], numeric_port);
    subscriber(client_socket, arg_values[1]);
    close(client_socket);

    return EXIT_SUCCESS;
}