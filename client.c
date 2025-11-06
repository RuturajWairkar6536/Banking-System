#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // For read, write, close
#include <arpa/inet.h>  // For inet_addr
#include <sys/socket.h> // For socket, connect
#include <netinet/in.h> // For sockaddr_in

#define PORT 8080

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

    // 1. Create Socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 address from text to binary form
    // We connect to "127.0.0.1" (localhost)
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    // 2. Connect to Server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }
    
    printf("Connected to Bank Server.\n");

    // 3. Communication Loop
    while (1) {
        // Clear buffer and read server prompt
        memset(buffer, 0, sizeof(buffer));
        int read_val = read(sock, buffer, 1024 - 1); // Read n-1

        if (read_val <= 0) {
            printf("Server disconnected.\n");
            break;
        }
        buffer[read_val] = '\0'; // Null-terminate

        // Print the server's message (e.g., "Enter User ID: ")
        printf("%s", buffer);

        // Check for server-side disconnects
        if (strstr(buffer, "Logging out") != NULL) {
            break;
        }
        if (strstr(buffer, "already logged in") != NULL) {
            break;
        }
        if (strstr(buffer, "Server is full") != NULL) {
            break;
        }

        // Get user input from the keyboard
        fgets(buffer, 1024, stdin);
        
        // Send user's input to the server
        send(sock, buffer, strlen(buffer), 0);

        // This is the client-side "Exit"
        if (strncmp(buffer, "exit", 4) == 0) {
            printf("Exiting...\n");
            break;
        }
    }

    // 4. Close Socket
    close(sock);
    return 0;
}