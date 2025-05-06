#ifndef SUBSCRIBER_H
#define SUBSCRIBER_H

#include "common.h"

/**
 * @brief Receive a string from the socket
 * 
 * @param sockfd Socket file descriptor
 * @param len Length of the string to receive
 * @return std::string The received string
 */
std::string recv_string(int sockfd, int len);

/**
 * @brief Send a connection message to the server
 * 
 * @param sockfd Socket file descriptor
 * @param id Client ID
 */
void send_connect_message(int sockfd, const char* id);

/**
 * @brief Send a connection message to the server
 * 
 * @param sockfd Socket file descriptor
 * @param id Client ID
 */
void handle_server_message(int sockfd, const char* id, bool& running);

/**
 * @brief Send a connection message to the server
 * 
 * @param sockfd Socket file descriptor
 * @param id Client ID
 */
bool process_user_command(const char* cmd, int argc, char** argv, int sockfd, const char* id);

/**
 * @brief Send a connection message to the server
 * 
 * @param sockfd Socket file descriptor
 * @param id Client ID
 */
void handle_user_input(int sockfd, const char* id, bool& running);

/**
 * @brief Main subscriber loop
 * 
 * @param sockfd Socket connected to the server
 * @param id Client ID
 */
void subscriber(int sockfd, char *id);

/**
 * @brief Send a connection message to the server
 * 
 * @param sockfd Socket file descriptor
 * @param id Client ID
 */
void exit_on_failure(bool condition, const char* message);

/**
 * @brief Send a connection message to the server
 * 
 * @param sockfd Socket file descriptor
 * @param id Client ID
 */
int establish_connection(const char* ip_address, uint16_t port);

#endif // SUBSCRIBER_H