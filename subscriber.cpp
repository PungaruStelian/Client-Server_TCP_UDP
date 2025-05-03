#include "subscriber.h"

std::string recv_string(int sockfd, int len) {
    std::string result(len, '\0'); // Pre-allocate string with the right size
    int bytes_received = recv_all(sockfd, &result[0], len);
    
    if (bytes_received == 0) {
        return ""; // Connection closed
    }
    
    return result;
}

void subscriber(int sockfd, char *id)
{
    /**
     * Signal that a new subscriber is connecting by sending
     * a connection packet. This is default behaviour for all clients.
     */
    tcp_request_t connect;
    memset(&connect, 0, sizeof(connect));
    strcpy(connect.id, id);
    connect.type = MESSAGE;
    connect.message = CONNECT;
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

    char buf[MESSAGES_SIZE + 1];
    memset(buf, 0, MESSAGES_SIZE + 1);

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

                if (rc && request.message == SHUTDOWN)
                {
                    std::cout << "Server is shutting down. Closing connection.\n";
                    free(pfds);
                    return;
                }

                std::string buff(reinterpret_cast<char*>(&request), sizeof(request));
                parse_input(buff); // Use c_str() if parse_input needs a char*
            }
            else
            {
                // Read length prefix
                recv_all(sockfd, &len, sizeof(len));

                std::string buff = recv_string(sockfd, len);
                parse_input(buff); // Use c_str() if parse_input needs a char*
            }
        }
        else if (pfds[0].revents & POLLIN)
        {
            /* Subscriber sends a request */
            fgets(buf, sizeof(buf), stdin);

            /* Parse the input */
            char *argv[MESSAGES_SIZE];
            int argc;
            argc = string_to_argv(buf, argv);

            /* If the subscriber wishes to disconnect */
            if (!strcmp(argv[0], "exit"))
            {
                if (argc != 1)
                {
                    std::cout << "\nWrong format for exit\n";
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
                    std::cout << "\nWrong format for subscribe\n";
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

                    std::cout << "Subscribed to topic.\n";
                }
            }

            /* If the subscriber wishes to unsubscribe */
            if (!strcmp(argv[0], "unsubscribe"))
            {
                if (argc != 2)
                {
                    std::cout << "\nWrong format for unsubscribe\n";
                }
                else
                {
                    tcp_request_t request;
                    memset(&request, 0, sizeof(request));
                    strcpy(request.id, id);
                    request.type = UNSUBSCRIBE;
                    strcpy(request.subscribe.topic, argv[1]);

                    send_all(sockfd, &request, sizeof(request));

                    std::cout << "Unsubscribed from topic.\n";
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
        std::cerr << "\nUsage: " << argv[0] << " <ID_CLIENT> <IP_SERVER> <PORT_SERVER>\n";
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