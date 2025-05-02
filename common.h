#ifndef COMMON_H
#define COMMON_H

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cmath>

#include <iostream>
#include <iomanip>

/**
 * @brief Maximum size of a message buffer
 */
#define MESSAGES_SIZE 1500

/**
 * @brief Macro to handle errors
 * 
 * @param assertion The condition to check (error if true)
 * @param call_description Error message to display
 */
#define DIE(assertion, call_description)                                       \
    do {                                                                       \
        if (assertion) {                                                       \
            std::cerr << "(" << __FILE__ << ", " << __LINE__ << "): ";         \
            std::cerr << call_description << ": " << strerror(errno) << std::endl; \
            exit(errno);                                                       \
        }                                                                      \
    } while (0)

/**
 * @brief Types of requests that can be sent between client and server
 */
enum command_t {
    EXIT,               ///< Client requests to disconnect
    SUBSCRIBE,          ///< Client subscribes to a topic
    UNSUBSCRIBE,        ///< Client unsubscribes from a topic
    MESSAGE,          ///< Client sends a message
};

/**
 * @brief Types of system messages
 */
enum system_message_t {
    CONNECT,        ///< Client connects to the server
    SHUTDOWN        ///< Server notifies clients it's shutting down
};

/**
 * @brief Types of data that can be transmitted in messages
 */
enum data_t {
    INT = 0,            ///< Integer value
    SHORT_REAL = 1,     ///< Short real value (fixed 2 decimal places)
    FLOAT = 2,          ///< Float value (variable decimal places)
    STRING = 3          ///< String value
};

/**
 * @brief Structure for a subscription request
 */
struct subscribe_t {
    char topic[51];     ///< Topic name (max 50 chars + null terminator)
    bool sf;            ///< Store-and-forward flag
};

/**
 * @brief Structure for an unsubscription request
 */
struct unsubscribe_t {
    char topic[51];     ///< Topic name (max 50 chars + null terminator)
};

/**
 * @brief Structure for a TCP request
 */
struct tcp_request_t {
    char id[11];        ///< Client ID (max 10 chars + null terminator)
    union {
        subscribe_t subscribe;      ///< Subscribe request data
        unsubscribe_t unsubscribe;  ///< Unsubscribe request data
        system_message_t message;  ///< System message data
    };
    command_t type;  ///< Type of request (-1 for system messages)
};

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
int string_to_argv(char *buf, char **argv);

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
void parse_input(char *buff);

#endif // COMMON_H