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

// Constants and macros
#define MSG_MAXSIZE 1024

#define DIE(assertion, call_description)                                       \
    do {                                                                       \
        if (assertion) {                                                       \
            fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);                 \
            perror(call_description);                                          \
            exit(errno);                                                       \
        }                                                                      \
    } while (0)

// Enums and structs 
enum request_type {
    EXIT,
    SUBSCRIBE,
    UNSUBSCRIBE,
    CONNECT,
    SERVER_SHUTDOWN
};

enum data_type {
    INT = 0,
    SHORT_REAL = 1,
    FLOAT = 2,
    STRING = 3
};

struct subscribe_t {
    char topic[51];
    bool sf;
} __attribute__ ((__packed__));

struct unsubscribe_t {
    char topic[51];
} __attribute__ ((__packed__));

struct tcp_request_t {
    char id[11];
    union {
        subscribe_t subscribe;
        unsubscribe_t unsubscribe;
    };
    request_type type;
} __attribute__ ((__packed__));

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
 * @brief Receives exactly 'len' bytes from socket
 * 
 * @param sockfd Socket file descriptor
 * @param buffer Buffer to store received data
 * @param len Number of bytes to receive
 * @return int Number of bytes received or 0 on disconnect
 */
int recv_all(int sockfd, void *buffer, int len);

/**
 * @brief Sends exactly 'len' bytes to socket
 * 
 * @param sockfd Socket file descriptor
 * @param buffer Buffer containing data to send
 * @param len Number of bytes to send
 * @return int Number of bytes sent
 */
int send_all(int sockfd, void *buffer, int len);

/**
 * @brief Parses a string by whitespace into an array of arguments
 * 
 * @param buf String to parse
 * @param argv Array to store parsed arguments
 * @return int Number of arguments parsed
 */
int parse_by_whitespace(char *buf, char **argv);

/**
 * @brief Print an integer value from the buffer in the expected format
 * 
 * @param buff Buffer containing the integer value
 * @param topic Topic name
 */
void print_int(char *buff, char *topic);

/**
 * @brief Print a short real value from the buffer in the expected format
 * 
 * @param buff Buffer containing the short real value
 * @param topic Topic name
 */
void print_short_real(char *buff, char *topic);

/**
 * @brief Print a float value from the buffer in the expected format
 * 
 * @param buff Buffer containing the float value
 * @param topic Topic name
 */
void print_float(char *buff, char *topic);

/**
 * @brief Parse a subscription message and print it
 * 
 * @param buff Buffer containing the message
 */
void parse_subscription(char *buff);

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