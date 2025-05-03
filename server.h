#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>      // for in_addr, inet_ntoa
#include <netinet/in.h>     // for sockaddr_in
#include <netinet/tcp.h>    // for TCP_NODELAY
#include <poll.h>           // for poll, pollfd
#include <sys/socket.h>     // for socket functions
#include <unistd.h>         // for close, STDIN_FILENO

#include <algorithm>        // for std::remove
#include <iostream>         // for std::cout, std::cerr
#include <map>              // for std::map
#include <set>              // for std::set
#include <string>           // for std::string
#include <unordered_map>    // for std::unordered_map
#include <vector>           // for std::vector

#include <cstdlib>
#include <cstdio>

#include "common.h"

struct stored_message_t {
    int c;
    int len;
    std::string buff;
};

struct tcp_client_t {
    int fd;
    std::string id;
    bool connected;
    std::map<std::string, bool> topics;
    std::vector<stored_message_t *> lost_messages;
};

// Define a struct to hold all server state
struct ServerState {
    std::map<std::string, tcp_client_t*> clients;  // Maps client IDs to client info
    std::map<std::string, std::vector<tcp_client_t*>> subscriptions;  // Maps topics to subscribers
    std::map<int, std::pair<in_addr, uint16_t>> client_addresses;  // Maps socket FDs to client network info
};

/**
 * @brief Append binary data to a string
 * 
 * @param str String to append to
 * @param data Data to append
 * @param len Length of data
 */
void append_binary_data(std::string& str, const void* data, size_t len);

/**
 * @brief Check if a topic matches a pattern with wildcards
 * 
 * @param topic The actual topic string
 * @param pattern The pattern with possible wildcards
 * @return true if the topic matches the pattern
 */
bool topic_matches_pattern(const std::string& topic, const std::string& pattern);

/**
 * @brief Handle a new TCP connection
 * 
 * @param listenfd Listening socket file descriptor
 * @param state Server state
 * @param poll_fds List of poll file descriptors
 */
void handle_new_connection(int listenfd, ServerState& state, std::vector<struct pollfd>& poll_fds);

/**
 * @brief Process a UDP message
 * 
 * @param udp_fd UDP socket file descriptor
 * @param state Server state
 */
void process_udp_message(int udp_fd, ServerState& state);

/**
 * @brief Handle a server command
 * 
 * @param state Server state
 * @param poll_fds List of poll file descriptors
 * @return true if the command was handled successfully
 */
bool handle_server_command(ServerState& state, std::vector<struct pollfd>& poll_fds);

/**
 * @brief Handle a client request
 * 
 * @param fd File descriptor of the client
 * @param request The request from the client
 * @param state Server state
 * @param poll_fds List of poll file descriptors
 * @param index Index of the client in the poll_fds array
 */
void handle_client_request(int fd, tcp_request_t& request, ServerState& state, 
    std::vector<struct pollfd>& poll_fds, uint64_t index);

/**
 * @brief Handle a client disconnecting
 * 
 * @param fd File descriptor of the client
 * @param state Server state
 * @param poll_fds List of poll file descriptors
 * @param index Index of the client in the poll_fds array
 */
void handle_client_disconnect(int fd, ServerState& state, std::vector<struct pollfd>& poll_fds, uint64_t index);

/**
 * @brief Main server loop
 * 
 * @param listenfd TCP listening socket
 * @param udp_cli_fd UDP socket
 */
void server(int listenfd, int udp_cli_fd);

/**
 * @brief Configure a socket for TCP or UDP
 * 
 * @param sock Socket file descriptor
 * @param type Socket type (SOCK_STREAM or SOCK_DGRAM)
 * @param port Port number to bind to
 */
void configure_socket(int& sock, int type, uint16_t port);

#endif // SERVER_H