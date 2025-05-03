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

int string_to_argv(char *buf, char **argv) {
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

void parse_input(const std::string& buff) {
    size_t pos = 0;

    // Skip IP (4 bytes) and port (2 bytes)
    pos += sizeof(in_addr) + sizeof(uint16_t);

    // Extract topic (50 bytes) and trim at first null
    std::string topic = buff.substr(pos, 50);
    size_t null_pos = topic.find('\0');
    // position found
    if (null_pos != std::string::npos) {
        topic.resize(null_pos);
    }
    pos += 50;

    // Validate remaining buffer size
    if (pos >= buff.size()) return;
        unsigned char type = buff[pos++];

    switch (type) {
        case INT: {
            if (pos + sizeof(char) + sizeof(uint32_t) > buff.size()) break;
            char sign = buff[pos++];
            
            uint32_t net_value;
            memcpy(&net_value, buff.data() + pos, sizeof(uint32_t));
            int value = ntohl(net_value);
            if (sign) value = -value;
            
            std::cout << topic << " - INT - " << value << "\n";
            pos += sizeof(uint32_t);
            break;
        }
        case SHORT_REAL: {
            if (pos + sizeof(uint16_t) > buff.size()) break;
            
            uint16_t net_value;
            memcpy(&net_value, buff.data() + pos, sizeof(uint16_t));
            float value = ntohs(net_value) / 100.0f;
            
            std::cout << topic << " - SHORT_REAL - " 
                      << std::fixed << std::setprecision(2) << value << "\n";
            pos += sizeof(uint16_t);
            break;
        }
        case FLOAT: {
            if (pos + sizeof(char) + sizeof(uint32_t) + sizeof(char) > buff.size()) break;
            char sign = buff[pos++];
            
            uint32_t net_value;
            memcpy(&net_value, buff.data() + pos, sizeof(uint32_t));
            pos += sizeof(uint32_t);
            char exponent = buff[pos++];
            
            float value = ntohl(net_value);
            if (sign) value = -value;
            value /= pow(10, exponent);
            
            std::cout << topic << " - FLOAT - " 
                      << std::fixed << std::setprecision(exponent) << value << "\n";
            break;
        }
        case STRING: {
            size_t end = buff.find('\0', pos);
            if (end == std::string::npos) end = buff.size();
            std::string payload = buff.substr(pos, end - pos);
            
            std::cout << topic << " - STRING - " << payload << "\n";
            break;
        }
        default:
            std::cerr << "Unknown data type\n";
    }
}