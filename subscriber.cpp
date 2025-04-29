#include "subscriber.h"

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

// Original subscriber.cpp functions
void subscriber(int sockfd, char *id)
{
    /**
     * Signal that a new subscriber is connecting by sending
     * a connection packet. This is default behaviour for all clients.
     */
    tcp_request_t connect;
    memset(&connect, 0, sizeof(connect));
    strcpy(connect.id, id);
    connect.type = CONNECT;
    send_all(sockfd, &connect, sizeof(connect));

    // Initial capacity for pfds array
    size_t pfds_capacity = 2; // Start with minimal capacity since we only need 2 initially
    size_t pfds_size = 0;

    // Dynamically allocate the array
    struct pollfd *pfds = (struct pollfd *)malloc(pfds_capacity * sizeof(struct pollfd));
    DIE(pfds == NULL, "malloc failed for pfds");

    /* stdin fd entry */
    pfds[pfds_size].fd = STDIN_FILENO;
    pfds[pfds_size].events = POLLIN;
    pfds[pfds_size].revents = 0;
    ++pfds_size;

    /* server socket fd entry */
    pfds[pfds_size].fd = sockfd;
    pfds[pfds_size].events = POLLIN;
    pfds[pfds_size].revents = 0;
    ++pfds_size;

    char buf[MSG_MAXSIZE + 1];
    memset(buf, 0, MSG_MAXSIZE + 1);

    while (1)
    {
        /* Wait for data on at least one fd */
        poll(pfds, pfds_size, -1);

        /* The server response */
        if (pfds[1].revents & POLLIN)
        {
            // Try to read message length first (UDP messages have length prefix)
            int len;
            int rc = recv(sockfd, &len, sizeof(len), MSG_PEEK);
            if (rc <= 0)
            {
                // Connection error or closed
                free(pfds);
                return;
            }

            // Special case: If exactly control message size, might be control message
            if (rc == sizeof(len) && len == sizeof(tcp_request_t))
            {
                // Could be a control message, read it fully
                tcp_request_t request;
                rc = recv_all(sockfd, &request, sizeof(request));

                if (rc && request.type == SERVER_SHUTDOWN)
                {
                    printf("Server is shutting down. Closing connection.\n");
                    free(pfds);
                    return;
                }

                // Not a shutdown message, it was actually a UDP message
                // We need to process the message now
                char buff[2 * MSG_MAXSIZE];
                memcpy(buff, &request, sizeof(request)); // Copy what we've read

                // Parse and display
                parse_subscription(buff);
            }
            else
            {
                // Normal UDP message
                char buff[2 * MSG_MAXSIZE];

                // Read length prefix
                recv_all(sockfd, &len, sizeof(len));

                // Read message content
                recv_all(sockfd, buff, len);

                // Parse and display
                parse_subscription(buff);
            }
        }
        else if (pfds[0].revents & POLLIN)
        {
            /* Subscriber sends a request */
            fgets(buf, sizeof(buf), stdin);

            /* Parse the input */
            char *argv[MSG_MAXSIZE];
            int argc;
            argc = parse_by_whitespace(buf, argv);

            /* If the subscriber wishes to disconnect */
            if (!strcmp(argv[0], "exit"))
            {
                if (argc != 1)
                {
                    printf("\nWrong format for exit\n");
                }
                else
                {
                    tcp_request_t request;
                    memset(&request, 0, sizeof(request));
                    strcpy(request.id, id);
                    request.type = EXIT;

                    send_all(sockfd, &request, sizeof(request));
                    free(pfds); // Free the dynamically allocated memory
                    return;
                }
            }

            /* If the subscriber wishes to subscribe */
            if (!strcmp(argv[0], "subscribe"))
            {
                if (argc < 2)
                {
                    printf("\nWrong format for subscribe\n");
                }
                else
                {
                    // If needed to add a new fd (example, not actually needed here)
                    if (pfds_size >= pfds_capacity)
                    {
                        // Double the capacity when full
                        pfds_capacity *= 2;
                        struct pollfd *new_pfds = (struct pollfd *)realloc(pfds, pfds_capacity * sizeof(struct pollfd));
                        DIE(new_pfds == NULL, "realloc failed for pfds");
                        pfds = new_pfds;
                    }

                    tcp_request_t request;
                    memset(&request, 0, sizeof(request));
                    strcpy(request.id, id);
                    request.type = SUBSCRIBE;
                    strcpy(request.subscribe.topic, argv[1]);

                    // Set default sf=0 if not provided
                    if (argc >= 3)
                    {
                        request.subscribe.sf = atoi(argv[2]);
                    }
                    else
                    {
                        request.subscribe.sf = 0; // Default to no store-and-forward
                    }

                    send_all(sockfd, &request, sizeof(request));

                    printf("Subscribed to topic.\n");
                }
            }

            /* If the subscriber wishes to unsubscribe */
            if (!strcmp(argv[0], "unsubscribe"))
            {
                if (argc != 2)
                {
                    printf("\nWrong format for unsubscribe\n");
                }
                else
                {
                    tcp_request_t request;
                    memset(&request, 0, sizeof(request));
                    strcpy(request.id, id);
                    request.type = UNSUBSCRIBE;
                    strcpy(request.subscribe.topic, argv[1]);

                    send_all(sockfd, &request, sizeof(request));

                    printf("Unsubscribed from topic.\n");
                }
            }
        }
    }
    free(pfds); // Free the dynamically allocated memory
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "\nUsage: %s <ID_CLIENT> <IP_SERVER> <PORT_SERVER>\n",
                argv[0]);
        return 1;
    }

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    uint16_t server_port;
    int rc;
    rc = sscanf(argv[3], "%hu", &server_port);
    DIE(rc != 1, "sscanf() failed");

    int sockfd = -1;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket() failed");

    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    // Setează opțiunea SO_REUSEADDR
    int enable = 1;
    rc = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    DIE(rc < 0, "setsockopt(SO_REUSEADDR) failed");

    // Setează opțiunea TCP_NODELAY (dezactivează algoritmul lui Nagle)
    rc = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
    DIE(rc < 0, "setsockopt(TCP_NODELAY) failed");

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);
    rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
    DIE(rc <= 0, "inet_pton() failed");

    rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "connect() failed");

    subscriber(sockfd, argv[1]);

    close(sockfd);

    return 0;
}