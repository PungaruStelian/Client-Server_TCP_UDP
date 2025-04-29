#include "server.h"

// Global variables
std::map<std::string, tcp_client_t *> ids;
std::map<std::string, std::vector<tcp_client_t *>> topics;
std::map<int, std::pair<in_addr, uint16_t>> ips_ports;

int recv_all(int sockfd, void *buffer, int len) {
    int bytes_received = 0;
    int bytes_remaining = len;
    char *buff = (char *)buffer;

    while (bytes_remaining > 0) {
        int rc = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
        DIE(rc == -1, "recv() failed");

        if (!rc)
            return 0;

        bytes_received += rc;
        bytes_remaining -= rc;
    }

    return bytes_received;
}

int send_all(int sockfd, void *buffer, int len) {
    int bytes_sent = 0;
    int bytes_remaining = len;
    char *buff = (char *)buffer;

    while (bytes_remaining > 0) {
        int rc = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
        DIE(rc == -1, "send() failed");

        bytes_sent += rc;
        bytes_remaining -= rc;
    }

    return bytes_sent;
}

// Functions from parser.cpp
int parse_by_whitespace(char *buf, char **argv) {
    int argc = 0;
    for (char *p = strtok(buf, " \t\n"); p; p = strtok(NULL, " \t\n"))
        argv[argc++] = p;
    return argc;
}

void print_int(char *buff, char *topic) {
    int8_t sign = *buff;
    buff += sizeof(sign);

    int64_t nr = ntohl(*(u_int32_t *)buff);

    if (sign)
        nr = -nr;

    printf("%s - INT - %ld\n", topic, nr);
}

void print_short_real(char *buff, char *topic) {
    float nr = ntohs(*(u_int16_t *)buff) / (float)100;

    printf("%s - SHORT_REAL - %.2f\n", topic, nr);
}

void print_float(char *buff, char *topic) {
    int8_t sign = *buff;
    buff += sizeof(sign);

    float nr = ntohl(*(u_int32_t *)buff);
    buff += sizeof(uint32_t);

    if (sign)
        nr = -nr;

    int8_t pow10 = *buff;

    printf("%s - FLOAT - %.4f\n", topic, nr / (float) pow(10, pow10));
}

void parse_subscription(char *buff) {
    // Skip the prepended IP address (4 bytes) and port (2 bytes)
    struct in_addr udp_ip;
    uint16_t udp_port;

    memcpy(&udp_ip, buff, sizeof(struct in_addr));
    buff += sizeof(struct in_addr);

    memcpy(&udp_port, buff, sizeof(uint16_t));
    buff += sizeof(uint16_t);

    // Now 'buff' points to the start of the original UDP payload (topic)
    char topic[51] = { 0 };
    memcpy(topic, buff, 50);
    buff += 50; // Move buffer pointer past the topic

    u_int8_t type = *buff;
    buff += sizeof(u_int8_t); // Move buffer pointer past the type

    // Print the source UDP client info along with the message
    printf("%s:%hu - ", inet_ntoa(udp_ip), ntohs(udp_port));

    switch (type) {
    case INT:
        print_int(buff, topic);
        break;
    case SHORT_REAL:
        print_short_real(buff, topic);
        break;
    case FLOAT:
        print_float(buff, topic);
        break;
    case STRING:
        // The rest of the buffer is the string content
        printf("%s - STRING - %s\n", topic, buff);
        break;
    default:
        fprintf(stderr, "Unknown data type received.\n");
        break;
    }
}

// Original server.cpp functions
bool topic_matches_pattern(const std::string& topic, const std::string& pattern) {
    // Split both topic and pattern by '/'
    std::vector<std::string> topic_parts;
    std::vector<std::string> pattern_parts;
    std::stringstream ss_topic(topic);
    std::stringstream ss_pattern(pattern);
    std::string part;

    while (getline(ss_topic, part, '/')) {
        topic_parts.push_back(part);
    }
    while (getline(ss_pattern, part, '/')) {
        pattern_parts.push_back(part);
    }

    // Recursive helper function to handle complex wildcard matching
    std::function<bool(size_t, size_t)> match = [&](size_t t_idx, size_t p_idx) -> bool {
        // Base case: reached the end of both strings
        if (t_idx == topic_parts.size() && p_idx == pattern_parts.size()) {
            return true;
        }
        
        // If we've reached the end of the pattern but not the topic, no match
        if (p_idx == pattern_parts.size()) {
            return false;
        }
        
        // If we've reached the end of topic but not pattern, only "*" can match
        if (t_idx == topic_parts.size()) {
            // Only "*" can match with empty topic parts
            return pattern_parts[p_idx] == "*" && match(t_idx, p_idx + 1);
        }
        
        // Handle different pattern cases
        if (pattern_parts[p_idx] == "+") {
            // '+' matches exactly one level
            return match(t_idx + 1, p_idx + 1);
        } 
        else if (pattern_parts[p_idx] == "*") {
            // '*' can match zero or more levels
            
            // Try matching "*" with zero levels (skip the "*")
            if (match(t_idx, p_idx + 1)) {
                return true;
            }
            
            // Try matching "*" with one or more levels
            return match(t_idx + 1, p_idx);
        } 
        else {
            // Regular string matching
            if (pattern_parts[p_idx] == topic_parts[t_idx]) {
                return match(t_idx + 1, p_idx + 1);
            }
            return false;
        }
    };

    return match(0, 0);
}

void send_message(stored_message_t *message, int fd) {
    send_all(fd, &message->len, sizeof(message->len));
    send_all(fd, message->buff, message->len);
}

void server(int listenfd, int udp_cli_fd) {
    // Original server function...
    std::vector<struct pollfd> poll_fds;

    poll_fds.push_back(pollfd{listenfd, POLLIN, 0});
    poll_fds.push_back(pollfd{udp_cli_fd, POLLIN, 0});
    poll_fds.push_back(pollfd{STDIN_FILENO, POLLIN, 0});

    while (1) {
        int rc = poll(&poll_fds[0], poll_fds.size(), -1);
        DIE(rc < 0, "poll");

        for (uint64_t i = 0; i < poll_fds.size(); ++i) {
            // Check if the current fd has data to read
            if (poll_fds[i].revents & POLLIN) {
                // Case 1: New TCP connection request on the listening socket
                if (poll_fds[i].fd == listenfd) {
                    struct sockaddr_in tcp_cli_addr;
                    socklen_t tcp_cli_len = sizeof(tcp_cli_addr);
                    int tcp_cli_fd = accept(listenfd,
                               (struct sockaddr*)&tcp_cli_addr, &tcp_cli_len);
                    DIE(tcp_cli_fd < 0, "accept() failed");

                    // Disable Nagle's algorithm for low latency
                    int enable = 1;
                    int result = setsockopt(tcp_cli_fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
                    DIE(result < 0, "setsockopt TCP_NODELAY failed");

                    // Store client IP and port for identification
                    ips_ports[tcp_cli_fd] = std::make_pair(tcp_cli_addr.sin_addr,
                                                           tcp_cli_addr.sin_port);

                    // Add the new client socket to the poll set
                    poll_fds.push_back(pollfd{tcp_cli_fd, POLLIN, 0});

                // Case 2: UDP message received
                } else if (poll_fds[i].fd == udp_cli_fd) {
                    char buff[2 * MSG_MAXSIZE]; // Declare buffer for recvfrom
                    struct sockaddr_in udp_cli_addr;
                    socklen_t udp_cli_len = sizeof(udp_cli_addr);

                    // Receive UDP datagram
                    int bytes_received = recvfrom(poll_fds[i].fd, buff, sizeof(buff) -1, 0,
                                      (struct sockaddr*)&udp_cli_addr, &udp_cli_len);
                    DIE(bytes_received < 0, "recvfrom() failed");

                    // Calculate the total length to be sent to the subscriber:
                    // IP (4) + Port (2) + Original UDP Payload (bytes_received)
                    int total_len_for_subscriber = sizeof(in_addr_t) + sizeof(uint16_t) + bytes_received;

                    // Create a structure to store the message
                    stored_message_t *message = new stored_message_t; // Declare and initialize message
                    message->len = total_len_for_subscriber;
                    message->c = 0; // Reference count for SF clients
                    // Allocate buffer for the complete message (IP + Port + Payload)
                    message->buff = new char[message->len];

                    // Prepend UDP client IP and port to the message buffer
                    uint16_t udp_port_n = udp_cli_addr.sin_port;
                    in_addr_t udp_ip_n = udp_cli_addr.sin_addr.s_addr;
                    memcpy(message->buff, &udp_ip_n, sizeof(in_addr_t));
                    memcpy(message->buff + sizeof(in_addr_t), &udp_port_n, sizeof(uint16_t));
                    // Copy the actual UDP payload after IP and port
                    memcpy(message->buff + sizeof(in_addr_t) + sizeof(uint16_t), buff, bytes_received);

                    // Extract topic (first 50 bytes of the *original* UDP payload)
                    char topic_cstr[51];
                    memcpy(topic_cstr, buff, 50); // Now 'buff' is declared and contains UDP data
                    topic_cstr[50] = '\0'; // Ensure null termination
                    std::string current_topic_str(topic_cstr);

                    // Keep track of clients that have received this message to avoid duplicates
                    std::unordered_set<tcp_client_t*> clients_sent_to;

                    /* Iterate through all stored topic patterns (including wildcards) */
                    for (auto const& [pattern_str, subscribed_clients] : topics) {
                        // Check if the current message topic matches the stored pattern
                        if (topic_matches_pattern(current_topic_str, pattern_str)) {
                            for (auto &client : subscribed_clients) {
                                // Check if this client is actually subscribed to this specific pattern
                                if (client->topics.count(pattern_str)) {
                                    /**
                                     * When the client is connected, send the message instantly,
                                     * but only if it hasn't received it already for this UDP packet.
                                     */
                                    if (client->connected) {
                                        // Attempt to insert the client into the set.
                                        // If insertion is successful (true), the client hasn't received the message yet.
                                        if (clients_sent_to.insert(client).second) {
                                            send_message(message, client->fd);
                                        }
                                    }
                                    /**
                                     * When the client is not connected, but wishes to get
                                     * the message (SF=true for this pattern), store it.
                                     * Check if already queued to avoid duplicate storage entries if multiple patterns match.
                                     */
                                    else if (client->topics[pattern_str]) { // SF is true
                                        bool already_queued = false;
                                        for(const auto& msg : client->lost_messages) {
                                            if (msg == message) {
                                                already_queued = true;
                                                break;
                                            }
                                        }
                                        if (!already_queued) {
                                            ++message->c; // Increment ref count only if adding it
                                            client->lost_messages.push_back(message);
                                        }
                                    }
                                }
                            }
                        }
                    }

                    /* If no disconnected client needs this message, delete it */
                    if (message->c == 0) {
                        delete[] message->buff; // Free the buffer first
                        delete message;         // Then free the struct
                    }

                // Case 3: Input from server's stdin (e.g., "exit" command)
                } else if (poll_fds[i].fd == STDIN_FILENO) {
                    char buff[MSG_MAXSIZE], *argv[MSG_MAXSIZE];
                    fgets(buff, MSG_MAXSIZE, stdin);
                    int argc = parse_by_whitespace(buff, argv);

                    // Handle "exit" command
                    if (argc == 1 && strcmp(argv[0], "exit") == 0) {
                        // Send shutdown notice to all connected clients
                        for (auto const& [id, client_ptr] : ids) {
                            if (client_ptr->connected) {
                                tcp_request_t shutdown_notice;
                                memset(&shutdown_notice, 0, sizeof(shutdown_notice));
                                strcpy(shutdown_notice.id, "SERVER");
                                shutdown_notice.type = SERVER_SHUTDOWN;
                                send_all(client_ptr->fd, &shutdown_notice, sizeof(shutdown_notice));
                            }
                        }
                        
                        // Wait briefly for messages to be delivered
                        usleep(100000);  // 100ms
                        
                        // Close all sockets
                        for (const auto &p : poll_fds) {
                            if (p.fd != STDIN_FILENO) {
                                close(p.fd);
                            }
                        }
                        
                        // Clean up client data structures
                        for (auto const& [id, client_ptr] : ids) {
                            for (auto msg : client_ptr->lost_messages) {
                                // Decrement count or delete if unique owner
                                if (--msg->c == 0) {
                                    delete[] msg->buff;
                                    delete msg;
                                }
                            }
                            delete client_ptr;
                        }
                        
                        ids.clear();
                        topics.clear();
                        ips_ports.clear();
                        return; // Exit the server loop
                    } else {
                         printf("Unknown command.\n");
                    }

                // Case 4: Data received from a connected TCP client
                } else {
                    tcp_request_t request;
                    int bytes_recvd = recv_all(poll_fds[i].fd, &request, sizeof(request));

                    // Handle client disconnection (recv returns 0)
                    if (bytes_recvd == 0) {
                         // Find the client ID associated with this fd
                        std::string client_id_to_remove = "";
                        for (auto const& [id, client_ptr] : ids) {
                            if (client_ptr->fd == poll_fds[i].fd) {
                                client_id_to_remove = id;
                                break;
                            }
                        }

                        if (!client_id_to_remove.empty() && ids.count(client_id_to_remove)) {
                            printf("Client %s disconnected unexpectedly.\n", client_id_to_remove.c_str());
                            ids[client_id_to_remove]->connected = false;
                            // Don't remove from 'ids' map, just mark as disconnected
                        } else {
                             printf("Unknown client disconnected on fd %d.\n", poll_fds[i].fd);
                        }

                        close(poll_fds[i].fd);
                        ips_ports.erase(poll_fds[i].fd); // Remove from IP/port map
                        poll_fds.erase(poll_fds.begin() + i); // Remove from poll set
                        i--; // Adjust loop index after removal
                        continue; // Skip further processing for this fd
                    }
                    DIE(bytes_recvd < 0, "recv_all from client failed");


                    // Process the received request
                    switch (request.type) {
                    case CONNECT: {
                        request.id[10] = '\0'; // Ensure null termination
                        std::string client_id_str(request.id);

                        if (ids.count(client_id_str)) {
                            // Client ID exists
                            tcp_client_t *client = ids[client_id_str];
                            if (client->connected) {
                                // ID already connected, reject new connection
                                printf("Client %s already connected.\n", client_id_str.c_str());
                                close(poll_fds[i].fd); // Close the new socket
                                ips_ports.erase(poll_fds[i].fd);
                                poll_fds.erase(poll_fds.begin() + i);
                                i--; // Adjust loop index
                            } else {
                                // Client is reconnecting
                                printf("New client %s connected from %s:%hu.\n", 
                                       client_id_str.c_str(),
                                       inet_ntoa(ips_ports[poll_fds[i].fd].first),
                                       ntohs(ips_ports[poll_fds[i].fd].second));

                                // Update client state
                                client->fd = poll_fds[i].fd; // Update fd in case it changed (unlikely here but good practice)
                                client->connected = true;

                                // Send stored messages (SF)
                                for (auto &message : client->lost_messages) {
                                    send_message(message, client->fd);
                                    // Decrement reference count and delete if zero
                                    if (--message->c == 0) {
                                        delete[] message->buff;
                                        delete message;
                                    }
                                }
                                client->lost_messages.clear(); // Clear the queue
                            }
                        } else {
                            // New client ID
                            printf("New client %s connected from %s:%hu.\n", 
                                   client_id_str.c_str(),
                                   inet_ntoa(ips_ports[poll_fds[i].fd].first),
                                   ntohs(ips_ports[poll_fds[i].fd].second));

                            // Create new client entry
                            tcp_client_t *new_client = new tcp_client_t;
                            new_client->fd = poll_fds[i].fd;
                            new_client->id = client_id_str;
                            new_client->connected = true;

                            ids.insert({client_id_str, new_client});
                        }
                        break;
                    } // End case CONNECT

                    case SUBSCRIBE: {
                        request.id[10] = '\0'; // Ensure null termination
                        request.subscribe.topic[50] = '\0'; // Ensure null termination
                        std::string client_id_str(request.id);
                        std::string topic_str(request.subscribe.topic);

                        if (ids.count(client_id_str)) {
                            tcp_client_t *client = ids[client_id_str];

                            // Add client to the topic's subscriber list if not already there for this pattern
                            bool found = false;
                            if (topics.count(topic_str)) {
                                for(const auto& existing_client : topics[topic_str]) {
                                    if (existing_client == client) {
                                        found = true;
                                        break;
                                    }
                                }
                            }
                            if (!found) {
                                topics[topic_str].push_back(client);
                            }

                            // Update/add the topic and SF flag in the client's map
                            client->topics[topic_str] = request.subscribe.sf;
                        } else {
                             fprintf(stderr, "Subscribe request from unknown client ID: %s\n", client_id_str.c_str());
                        }
                        break;
                    } // End case SUBSCRIBE

                    case UNSUBSCRIBE: {
                        request.id[10] = '\0'; // Ensure null termination
                        request.unsubscribe.topic[50] = '\0'; // Ensure null termination
                        std::string client_id_str(request.id);
                        std::string topic_str(request.unsubscribe.topic);

                        if (ids.count(client_id_str)) {
                            tcp_client_t *client = ids[client_id_str];

                            // Remove client from the topic's subscriber list
                            if (topics.count(topic_str)) {
                                auto &subs = topics[topic_str];
                                // Use erase-remove idiom
                                subs.erase(std::remove(subs.begin(), subs.end(), client), subs.end());
                            }

                            // Remove the topic from the client's map
                            client->topics.erase(topic_str);
                        } else {
                             fprintf(stderr, "Unsubscribe request from unknown client ID: %s\n", client_id_str.c_str());
                        }
                        break;
                    } // End case UNSUBSCRIBE

                    case EXIT: {
                        request.id[10] = '\0'; // Ensure null termination
                        std::string client_id_str(request.id);

                        if (ids.count(client_id_str)) {
                            printf("Client %s disconnected.\n", client_id_str.c_str());
                            ids[client_id_str]->connected = false;
                            // Don't remove from 'ids' map, just mark disconnected
                        } else {
                             fprintf(stderr, "Exit request from unknown client ID: %s\n", client_id_str.c_str());
                        }
                        // Close the socket and remove from poll set
                        close(poll_fds[i].fd);
                        ips_ports.erase(poll_fds[i].fd);
                        poll_fds.erase(poll_fds.begin() + i);
                        i--; // Adjust loop index
                        break;
                    } // End case EXIT

                    default:
                        fprintf(stderr, "Unknown request type received from fd %d\n", poll_fds[i].fd);
                        break;
                    } // End switch (request.type)
                } // End else (data from TCP client)
            } // End if (poll_fds[i].revents & POLLIN)
             else if (poll_fds[i].revents & (POLLHUP | POLLERR)) {
                 // Handle hangup or error on a socket
                 int fd_to_close = poll_fds[i].fd;
                 fprintf(stderr, "Error or hangup on fd %d\n", fd_to_close);

                 // Find client ID if it's a client socket
                 std::string client_id_to_mark = "";
                 for (auto const& [id, client_ptr] : ids) {
                     if (client_ptr->fd == fd_to_close) {
                         client_id_to_mark = id;
                         break;
                     }
                 }
                 if (!client_id_to_mark.empty()) {
                     ids[client_id_to_mark]->connected = false;
                     printf("Client %s marked as disconnected due to error/hangup.\n", client_id_to_mark.c_str());
                 }

                 close(fd_to_close);
                 ips_ports.erase(fd_to_close);
                 poll_fds.erase(poll_fds.begin() + i);
                 i--; // Adjust loop index
             }
        } // End for loop through poll_fds
    } // End while(1)
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "\nUsage: %s <PORT_SERVER>\n", argv[0]);
        return 1;
    }

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    uint16_t server_port;
    int rc = sscanf(argv[1], "%hu", &server_port);
    DIE(rc != 1, "sscanf() failed");

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(listenfd < 0, "socket() failed");

    int udp_cli_fd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udp_cli_fd < 0, "socket() failed");

    // Set socket options to allow address reuse for both sockets
    int enable = 1;
    rc = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    DIE(rc < 0, "setsockopt(SO_REUSEADDR) failed for TCP");
    
    rc = setsockopt(udp_cli_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    DIE(rc < 0, "setsockopt(SO_REUSEADDR) failed for UDP");

    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    rc = bind(listenfd, (const struct sockaddr*)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "bind() failed");

    rc = bind(udp_cli_fd, (const struct sockaddr*)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "bind() failed");

    rc = listen(listenfd, SOMAXCONN);
    DIE(rc < 0, "listen() failed");

    server(listenfd, udp_cli_fd);

    close(listenfd);
    close(udp_cli_fd);
    return 0;
}