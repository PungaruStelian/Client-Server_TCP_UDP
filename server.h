#ifndef SERVER_H
#define SERVER_H

#include <bits/stdc++.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <netinet/tcp.h>
#include <functional>

struct stored_message_t {
    int c;
    int len;
    char *buff;
} __attribute__ ((__packed__));

struct tcp_client_t {
    int fd;
    std::string id;
    bool connected;
    std::map<std::string, bool> topics;
    std::vector<stored_message_t *> lost_messages;
};

// External globals
extern std::map<std::string, tcp_client_t *> ids;
extern std::map<std::string, std::vector<tcp_client_t *>> topics;
extern std::map<int, std::pair<in_addr, uint16_t>> ips_ports;

/**
 * @brief Check if a topic matches a pattern with wildcards
 * 
 * @param topic The actual topic string
 * @param pattern The pattern with possible wildcards
 * @return true if the topic matches the pattern
 */
bool topic_matches_pattern(const std::string& topic, const std::string& pattern);

/**
 * @brief Send a stored message to a client
 * 
 * @param message The message to send
 * @param fd The client socket
 */
void send_message(stored_message_t *message, int fd);

/**
 * @brief Main server loop
 * 
 * @param listenfd TCP listening socket
 * @param udp_cli_fd UDP socket
 */
void server(int listenfd, int udp_cli_fd);

#endif // SERVER_H