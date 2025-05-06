#include "server.h"


// Global variables for client and topic management
std::unordered_map<std::string, tcp_client_t *> ids;           // Maps client IDs to client structures
std::unordered_map<std::string, std::vector<tcp_client_t *>> topics;  // Maps topics to lists of subscribed clients
std::unordered_map<int, std::pair<in_addr, uint16_t>> ips_ports;  // Maps socket file descriptors to client IP/port

void append_binary_data(std::string& str, const void* data, size_t len) {
    const char* char_data = static_cast<const char*>(data);
    str.append(char_data, len);
}

bool topic_matches_pattern(const std::string &topic, const std::string &pattern) {
    // Split topic and pattern into parts separated by '/'
    std::vector<std::string> topic_parts, pattern_parts;
    std::string part;
    
    // Split topic by '/' delimiter
    size_t start = 0, end;
    while ((end = topic.find('/', start)) != std::string::npos) {
        part = topic.substr(start, end - start);
        topic_parts.push_back(part);
        start = end + 1;
    }
    topic_parts.push_back(topic.substr(start));
    
    // Split pattern by '/' delimiter
    start = 0;
    while ((end = pattern.find('/', start)) != std::string::npos) {
        part = pattern.substr(start, end - start);
        pattern_parts.push_back(part);
        start = end + 1;
    }
    pattern_parts.push_back(pattern.substr(start));

    // Indices for traversing the parts
    size_t t_idx = 0, p_idx = 0;
    // Backtracking positions for '*' wildcard
    size_t t_back = 0, p_back = 0;
    bool backtrack = false;

    while (t_idx < topic_parts.size()) {
        if (p_idx < pattern_parts.size()) {
            // Case 1: Exact match or '+' wildcard (matches exactly one level)
            if (pattern_parts[p_idx] == "+" || pattern_parts[p_idx] == topic_parts[t_idx]) {
                t_idx++;
                p_idx++;
                continue;
            }
            // Case 2: '*' wildcard (matches any number of levels)
            if (pattern_parts[p_idx] == "*") {
                // Save position for backtracking
                t_back = t_idx;
                p_back = p_idx;
                p_idx++;
                backtrack = true;
                continue;
            }
        }
        
        // Case 3: Backtrack for '*' when match fails
        if (backtrack && p_back < pattern_parts.size()) {
            t_back++;
            t_idx = t_back;
            p_idx = p_back + 1;
            continue;
        }

        // No case matches, pattern doesn't match topic
        return false;
    }

    // Check if remaining pattern parts are only '*' wildcards
    while (p_idx < pattern_parts.size() && pattern_parts[p_idx] == "*") {
        p_idx++;
    }

    // All parts must be processed for a complete match
    return (t_idx == topic_parts.size() && p_idx == pattern_parts.size());
}

void handle_new_connection(int listenfd, ServerState& state, std::vector<struct pollfd>& poll_fds) {
    struct sockaddr_in tcp_cli_addr;
    socklen_t tcp_cli_len = sizeof(tcp_cli_addr);
    int tcp_cli_fd = accept(listenfd, (struct sockaddr*)&tcp_cli_addr, &tcp_cli_len);
    DIE(tcp_cli_fd < 0, "accept() failed");
    
    // Disable Nagle's algorithm for improved latency
    int enable = 1;
    int result = setsockopt(tcp_cli_fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
    DIE(result < 0, "setsockopt TCP_NODELAY failed");
    
    // Store client address info for future reference
    state.client_addresses[tcp_cli_fd] = std::make_pair(tcp_cli_addr.sin_addr, tcp_cli_addr.sin_port);
    
    // Add new client socket to poll set
    poll_fds.push_back({tcp_cli_fd, POLLIN, 0});
}

void process_udp_message(int udp_fd, ServerState& state) {
    char buff[2 * MESSAGES_SIZE];
    struct sockaddr_in udp_cli_addr;
    socklen_t udp_cli_len = sizeof(udp_cli_addr);
    
    int bytes_received = recvfrom(udp_fd, buff, sizeof(buff) - 1, 0,
                                  (struct sockaddr*)&udp_cli_addr, &udp_cli_len);
    DIE(bytes_received < 0, "recvfrom() failed");
    
    // Create message structure to store the UDP message
    int total_len = sizeof(in_addr_t) + sizeof(uint16_t) + bytes_received;
    stored_message_t* message = new stored_message_t;
    message->len = total_len;
    message->c = 0;  // Reference count (for memory management)
    message->buff.reserve(total_len);
    
    // Add UDP source info and payload to message buffer
    append_binary_data(message->buff, &udp_cli_addr.sin_addr.s_addr, sizeof(in_addr_t));
    append_binary_data(message->buff, &udp_cli_addr.sin_port, sizeof(uint16_t));
    append_binary_data(message->buff, buff, bytes_received);
    
    // Extract topic from the payload
    char topic_str[51];
    memcpy(topic_str, buff, 50);
    topic_str[50] = '\0';
    std::string current_topic = topic_str;
    
    // Track clients that already received this message (avoid duplicates)
    std::set<tcp_client_t*> message_recipients;
    
    // Distribute message to all matching subscribers
    for (const auto& [pattern, subscribers] : state.subscriptions) {
        if (topic_matches_pattern(current_topic, pattern)) {
            for (auto* client : subscribers) {
                if (!client->topics.count(pattern)) continue;
                
                if (client->connected) {
                    // Send to connected client (if not already sent)
                    if (message_recipients.insert(client).second) {
                        send_all(client->fd, &message->len, sizeof(int));
                        send_all(client->fd, (void*)message->buff.c_str(), message->len);
                    }
                } 
                else if (client->topics[pattern]) {
                    // Store for disconnected client with Store-and-Forward enabled
                    bool already_stored = false;
                    for (auto* stored_msg : client->lost_messages) {
                        if (stored_msg == message) {
                            already_stored = true;
                            break;
                        }
                    }
                    
                    if (!already_stored) {
                        ++message->c;  // Increment reference count
                        client->lost_messages.push_back(message);
                    }
                }
            }
        }
    }
    
    // Cleanup if no references to the message
    if (message->c == 0) {
        delete message;
    }
}

bool handle_server_command(ServerState& state, std::vector<struct pollfd>& poll_fds) {
    char buff[MESSAGES_SIZE];
    char* argv[MESSAGES_SIZE];
    
    fgets(buff, MESSAGES_SIZE, stdin);
    int argc = string_to_argv(buff, argv);
    
    if (argc == 1 && strcmp(argv[0], "exit") == 0) {
        // Send shutdown notice to all connected clients
        for (const auto& [id, client] : state.clients) {
            if (client->connected) {
                tcp_request_t notice = {};
                strcpy(notice.id, "SERVER");
                notice.type = MESSAGE;
                notice.message = SHUTDOWN;
                send_all(client->fd, &notice, sizeof(notice));
            }
        }
        
        // Close all sockets
        for (const auto& pfd : poll_fds) {
            if (pfd.fd != STDIN_FILENO) {
                close(pfd.fd);
            }
        }
        
        // Free allocated memory
        for (const auto& [id, client] : state.clients) {
            for (auto* msg : client->lost_messages) {
                if (--msg->c == 0) {
                    delete msg;
                }
            }
            delete client;
        }
        
        return true; // Signal to exit server loop
    }
    
    return false;
}

void handle_client_request(int fd, tcp_request_t& request, ServerState& state, 
                           std::vector<struct pollfd>& poll_fds, uint64_t index) {
    request.id[10] = '\0'; // Ensure client ID is null-terminated
    std::string client_id(request.id);
    
    switch (request.type) {
        case MESSAGE: {
            if (state.clients.count(client_id)) {
                tcp_client_t* client = state.clients[client_id];
                
                if (client->connected) {
                    // Client already connected - reject duplicate connection
                    std::cout << "Client " << client_id << " already connected.\n";
                    close(fd);
                    state.client_addresses.erase(fd);
                    poll_fds.erase(poll_fds.begin() + index);
                } else {
                    // Client reconnecting - update state and send missed messages
                    auto& [ip, port] = state.client_addresses[fd];
                    std::cout << "New client " << client_id << " connected from " 
                              << inet_ntoa(ip) << ":" << ntohs(port) << ".\n";
                    
                    client->fd = fd;
                    client->connected = true;
                    
                    // Send stored messages accumulated during disconnect
                    for (auto* msg : client->lost_messages) {
                        send_all(fd, &msg->len, sizeof(int));
                        send_all(fd, (void*)msg->buff.c_str(), msg->len);
                        
                        if (--msg->c == 0) {
                            delete msg;
                        }
                    }
                    client->lost_messages.clear();
                }
            } else {
                // New client connecting for the first time
                auto& [ip, port] = state.client_addresses[fd];
                std::cout << "New client " << client_id << " connected from " 
                          << inet_ntoa(ip) << ":" << ntohs(port) << ".\n";
                
                tcp_client_t* new_client = new tcp_client_t;
                new_client->fd = fd;
                new_client->id = client_id;
                new_client->connected = true;
                
                state.clients[client_id] = new_client;
            }
            break;
        }
        
        case SUBSCRIBE: {
            request.subscribe.topic[50] = '\0';  // Ensure topic is null-terminated
            std::string topic(request.subscribe.topic);
            
            if (state.clients.count(client_id)) {
                tcp_client_t* client = state.clients[client_id];
                
                // Add client to subscribers list if not already subscribed
                bool found = false;
                if (state.subscriptions.count(topic)) {
                    for (auto* sub : state.subscriptions[topic]) {
                        if (sub == client) {
                            found = true;
                            break;
                        }
                    }
                }
                
                if (!found) {
                    state.subscriptions[topic].push_back(client);
                }
                
                // Update client's topics map with store-and-forward flag
                client->topics[topic] = request.subscribe.sf;
            }
            break;
        }
        
        case UNSUBSCRIBE: {
            request.unsubscribe.topic[50] = '\0';  // Ensure topic is null-terminated
            std::string topic(request.unsubscribe.topic);
            
            if (state.clients.count(client_id)) {
                tcp_client_t* client = state.clients[client_id];
                
                // Remove client from subscribers list
                if (state.subscriptions.count(topic)) {
                    auto& subs = state.subscriptions[topic];
                    subs.erase(
                        std::remove(subs.begin(), subs.end(), client),
                        subs.end()
                    );
                }
                
                // Remove topic from client's subscription list
                client->topics.erase(topic);
            }
            break;
        }
        
        case EXIT: {
            if (state.clients.count(client_id)) {
                std::cout << "Client " << client_id << " disconnected.\n";
                state.clients[client_id]->connected = false;
            }
            
            close(fd);
            state.client_addresses.erase(fd);
            poll_fds.erase(poll_fds.begin() + index);
            break;
        }
    }
}

void handle_client_disconnect(int fd, ServerState& state, std::vector<struct pollfd>& poll_fds, uint64_t index) {
    std::string client_id_to_mark;
    
    // Find client ID by socket descriptor
    for (const auto& [id, client] : state.clients) {
        if (client->fd == fd) {
            client_id_to_mark = id;
            break;
        }
    }
    
    if (!client_id_to_mark.empty()) {
        state.clients[client_id_to_mark]->connected = false;
    }
    
    close(fd);
    state.client_addresses.erase(fd);
    poll_fds.erase(poll_fds.begin() + index);
}

void server(int tcp_listen_fd, int udp_fd) {
    ServerState state;
    std::vector<struct pollfd> poll_set;

    // Setup file descriptors to monitor for activity
    poll_set.reserve(3);
    poll_set.push_back({.fd = udp_fd, .events = POLLIN, .revents = 0});      // UDP messages
    poll_set.push_back({.fd = tcp_listen_fd, .events = POLLIN, .revents = 0}); // TCP connections
    poll_set.push_back({.fd = STDIN_FILENO, .events = POLLIN, .revents = 0});  // Console input

    // Main event processing loop
    while (true) {
        int active_fds = poll(poll_set.data(), poll_set.size(), -1);
        DIE(active_fds < 0, "poll() error");

        for (size_t idx = 0; idx < poll_set.size(); ++idx) {
            struct pollfd& pfd = poll_set[idx];
            
            // Check for errors/hangups first
            if (pfd.revents & (POLLERR | POLLHUP)) {
                if (pfd.fd != tcp_listen_fd && pfd.fd != udp_fd && pfd.fd != STDIN_FILENO) {
                    handle_client_disconnect(pfd.fd, state, poll_set, idx);
                    idx--; // Adjust index after removing descriptor from array
                }
                continue;
            }

            if (!(pfd.revents & POLLIN)) 
                continue;

            // Handle event based on descriptor type
            if (pfd.fd == udp_fd) {
                process_udp_message(udp_fd, state); // Process UDP datagram
            } 
            else if (pfd.fd == tcp_listen_fd) {
                handle_new_connection(tcp_listen_fd, state, poll_set); // Accept new TCP connection
            } 
            else if (pfd.fd == STDIN_FILENO) {
                if (handle_server_command(state, poll_set)) // Process server console command
                    return;
            } 
            else {
                // Handle TCP client request
                tcp_request_t req;
                int bytes = recv_all(pfd.fd, &req, sizeof(req));

                if (bytes == 0) {
                    // Client closed connection
                    handle_client_disconnect(pfd.fd, state, poll_set, idx);
                    idx--;
                } 
                else if (bytes > 0) {
                    // Process client request
                    handle_client_request(pfd.fd, req, state, poll_set, idx);
                } 
                else {
                    DIE(true, "recv_all() failure");
                }
            }
        }
    }
}

void configure_socket(int& sock, int type, uint16_t port) {
    sock = socket(AF_INET, type, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Enable address reuse to avoid "address already in use" errors
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("Socket option failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Prepare and bind to server address
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Binding failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // For TCP sockets, start listening for connections
    if (type == SOCK_STREAM && listen(sock, SOMAXCONN) < 0) {
        perror("Listen failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
}

int main(int param_count, char* param_values[]) {
    // Check command-line arguments
    if (param_count != 2) {
        std::cerr << "Usage: " << param_values[0] << " <PORT>\n";
        return EXIT_FAILURE;
    }

    // Disable output buffering for immediate console output
    setvbuf(stdout, nullptr, _IONBF, 0);

    // Parse and validate port number
    char* end_ptr;
    const long port_num = strtol(param_values[1], &end_ptr, 10);
    if (*end_ptr != '\0' || port_num < 1024 || port_num > 65535) {
        std::cerr << "Invalid port number\n";
        return EXIT_FAILURE;
    }

    int tcp_sock, udp_sock;
    try {
        // Set up TCP and UDP sockets
        configure_socket(tcp_sock, SOCK_STREAM, port_num);
        configure_socket(udp_sock, SOCK_DGRAM, port_num);
        
        // Run the server
        server(tcp_sock, udp_sock);
        
        // Clean up resources
        close(tcp_sock);
        close(udp_sock);
    } catch (...) {
        std::cerr << "Runtime exception occurred\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}