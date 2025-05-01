#include "common.h"

int recv_all(int sockfd, void *buffer, int len) {
    int bytes_received = 0;
    int bytes_remaining = len;

    while (bytes_remaining > 0) {
        int rc = recv(sockfd, (char *)buffer + bytes_received, bytes_remaining, 0);
        DIE(rc == -1, "recv() failed");
        
        if (rc == 0)
            return 0;
        
        bytes_received += rc;
        bytes_remaining -= rc;
    }

    return bytes_received;
}

int send_all(int sockfd, void *buffer, int len) {
    int bytes_sent = 0;
    int bytes_remaining = len;

    while (bytes_remaining > 0) {
        int rc = send(sockfd, (char *)buffer + bytes_sent, bytes_remaining, 0);
        DIE(rc == -1, "send() failed");

        bytes_sent += rc;
        bytes_remaining -= rc;
    }

    return bytes_sent;
}

int parse_by_whitespace(char *buf, char **argv) {
    int argc = 0;
    char *p, delim[3] = " \n";

    p = strtok(buf, delim);
    while (p) {
        argv[argc] = p;
        argc++;
        p = strtok(NULL, delim);
    }

    return argc;
}

void print_int(char *buff, char *topic) {
    char sign = *buff;
    buff += sizeof(sign);

    int value = ntohl(*(unsigned int *)buff);

    if (sign)
        value = -value;

    printf("%s - INT - %d\n", topic, value);
}

void print_short_real(char *buff, char *topic) {
    float nr = (float)ntohs(*(unsigned short *)buff) / 100;
    printf("%s - SHORT_REAL - %.2f\n", topic, nr);
}

void print_float(char *buff, char *topic) {
    char sign = *buff;
    buff += sizeof(sign);

    float nr = ntohl(*(unsigned int *)buff);
    buff += sizeof(unsigned int);

    if (sign)
        nr = -nr;

    char pow10 = *buff;

    printf("%s - FLOAT - %.4f\n", topic, (float)nr / pow(10, pow10));
}

void parse_subscription(char *buff) {
    // Skip the prepended IP address (4 bytes) and port (2 bytes)
    in_addr udp_ip;
    unsigned short udp_port;

    memcpy(&udp_ip, buff, sizeof(unsigned int));
    buff += sizeof(unsigned int);

    memcpy(&udp_port, buff, sizeof(unsigned short));
    buff += sizeof(unsigned short);

    // Now 'buff' points to the start of the original UDP payload (topic)
    char topic[51];
    strncpy(topic, buff, 50);
    topic[50] = '\0'; // Ensure null termination
    buff += 50; // Move buffer pointer past the topic

    unsigned char type = *buff;
    buff += sizeof(unsigned char); // Move buffer pointer past the type

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
        fprintf(stderr, "Unknown data type.\n");
        break;
    }
}