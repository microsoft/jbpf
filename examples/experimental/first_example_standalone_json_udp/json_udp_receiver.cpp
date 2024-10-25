#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

int
main()
{
    // Create a UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }

    // Server address configuration
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8888);

    // Bind the socket to the server address
    if (bind(sockfd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        close(sockfd);
        return 1;
    }

    std::cout << "Waiting for incoming jbpf messages..." << std::endl;

    // Buffer for receiving data
    char buffer[2048];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Receive data from the client
    while (true) {
        int received_bytes = recvfrom(
            sockfd, buffer, sizeof(buffer) - 1, 0, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
        if (received_bytes < 0) {
            std::cerr << "Failed to receive data" << std::endl;
            close(sockfd);
            return 1;
        }

        // Null-terminate the received data
        buffer[received_bytes] = '\0';

        // The first 16 bytes contain the stream-id, so ignore and
        // read the payload from offset +16
        std::string cpp_string = std::string(buffer + 16);

        // Print the received message
        std::cout << cpp_string << std::endl;
    }

    // Close the socket
    close(sockfd);

    return 0;
}
