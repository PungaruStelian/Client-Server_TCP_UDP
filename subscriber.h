#ifndef SUBSCRIBER_H
#define SUBSCRIBER_H

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <inttypes.h>
#include <netinet/tcp.h>
#include <cmath>

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
 * @brief Main subscriber loop
 * 
 * @param sockfd Socket connected to the server
 * @param id Client ID
 */
void subscriber(int sockfd, char *id);

#endif // SUBSCRIBER_H