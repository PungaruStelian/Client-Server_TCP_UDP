#include "common.h"

int recv_all(int sockfd, void *buffer, int len) {
    char *buf_ptr = static_cast<char*>(buffer);
    int total = 0;
    
    do {
        int rc = recv(sockfd, buf_ptr + total, len - total, 0);
        DIE(rc == -1, "receive failure");
        
        if(rc == 0) return 0; // Connection closed by peer
        total += rc;
    } while(total < len); // Continue until buffer is filled

    return total;
}

int send_all(int sockfd, void *buffer, int len) {
    const char *data = static_cast<const char*>(buffer);
    int transmitted = 0;
    
    while(transmitted != len) {
        int rc = send(sockfd, data + transmitted, len - transmitted, 0);
        DIE(rc == -1, "transmission failure");
        
        transmitted += rc;
    }
    
    return transmitted;
}

// Parse a string into an array of arguments, similar to how main() receives argv
// Returns the number of arguments found
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

    // Skip network metadata (source address information)
    // UDP source IP (4 bytes) and port (2 bytes)
    pos += sizeof(in_addr) + sizeof(uint16_t);

    // Extract topic (fixed 50 byte field) and trim at first null character
    std::string topic = buff.substr(pos, 50);
    size_t null_pos = topic.find('\0');
    if (null_pos != std::string::npos) {
        topic.resize(null_pos);
    }
    pos += 50;

    // Ensure there's still data to read
    if (pos >= buff.size()) return;
    
    // Read message type byte (INT, SHORT_REAL, FLOAT, or STRING)
    unsigned char type = buff[pos++];

    // Process message based on data type
    switch (type) {
        case INT: {
            // Integer format: 1 byte sign + 4 bytes integer (network byte order)
            if (pos + sizeof(char) + sizeof(uint32_t) > buff.size()) break;
            char sign = buff[pos++];
            
            // Extract integer value and convert from network byte order
            uint32_t net_value;
            memcpy(&net_value, buff.data() + pos, sizeof(uint32_t));
            int value = ntohl(net_value);
            if (sign) value = -value;
            
            std::cout << topic << " - INT - " << value << "\n";
            pos += sizeof(uint32_t);
            break;
        }
        case SHORT_REAL: {
            // Short real format: 2 bytes for fixed-point value with 2 decimal places
            if (pos + sizeof(uint16_t) > buff.size()) break;
            
            // Extract value and convert from network byte order
            // Division by 100 converts fixed-point to floating point
            uint16_t net_value;
            memcpy(&net_value, buff.data() + pos, sizeof(uint16_t));
            float value = ntohs(net_value) / 100.0f;
            
            std::cout << topic << " - SHORT_REAL - " 
                      << std::fixed << std::setprecision(2) << value << "\n";
            pos += sizeof(uint16_t);
            break;
        }
        case FLOAT: {
            // Float format: 1 byte sign + 4 bytes value + 1 byte exponent
            if (pos + sizeof(char) + sizeof(uint32_t) + sizeof(char) > buff.size()) break;
            char sign = buff[pos++];
            
            // Extract mantissa and exponent
            uint32_t net_value;
            memcpy(&net_value, buff.data() + pos, sizeof(uint32_t));
            pos += sizeof(uint32_t);
            char exponent = buff[pos++];
            
            // Calculate actual float value: mantissa / 10^exponent
            float value = ntohl(net_value);
            if (sign) value = -value;
            value /= pow(10, exponent);
            
            // Display with appropriate precision based on exponent
            std::cout << topic << " - FLOAT - " 
                      << std::fixed << std::setprecision(exponent) << value << "\n";
            break;
        }
        case STRING: {
            // String format: null-terminated string
            // Find end of string (null character or end of buffer)
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