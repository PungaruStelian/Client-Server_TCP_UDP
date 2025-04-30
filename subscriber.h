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


/**
 * @brief Main subscriber loop
 * 
 * @param sockfd Socket connected to the server
 * @param id Client ID
 */
void subscriber(int sockfd, char *id);

#endif // SUBSCRIBER_H