// broadcast_server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/select.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

int main() {
    int server_sock, new_sock, max_sd, sd, activity, valread;
    int client_sockets[MAX_CLIENTS] = {0};
    struct sockaddr_in address;
    socklen_t addrlen;
    char buffer[BUFFER_SIZE];

    fd_set readfds;

    // Create server socket
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Allow reuse of address
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_sock, 10) < 0) {
        perror("Listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    addrlen = sizeof(address);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_sock, &readfds);
        max_sd = server_sock;

        // Add child sockets to set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];
            if (sd > 0)
                FD_SET(sd, &readfds);
            if (sd > max_sd)
                max_sd = sd;
        }

        // Wait for activity
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0 && errno != EINTR) {
            perror("Select error");
        }

        // Incoming connection
        if (FD_ISSET(server_sock, &readfds)) {
            if ((new_sock = accept(server_sock, (struct sockaddr *)&address, &addrlen)) < 0) {
                perror("Accept");
                exit(EXIT_FAILURE);
            }

            printf("New connection: socket fd is %d, ip is %s, port %d\n",
                   new_sock,
                   inet_ntoa(address.sin_addr),
                   ntohs(address.sin_port));

            // Add new socket to array
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_sock;
                    printf("Added to client list at index %d\n", i);
                    break;
                }
            }
        }

        // Check for IO operations on clients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];

            if (FD_ISSET(sd, &readfds)) {
                valread = read(sd, buffer, BUFFER_SIZE);
                if (valread == 0) {
                    // Client disconnected
                    getpeername(sd, (struct sockaddr *)&address, &addrlen);
                    printf("Client disconnected: ip %s, port %d\n",
                           inet_ntoa(address.sin_addr),
                           ntohs(address.sin_port));
                    close(sd);
                    client_sockets[i] = 0;
                } else {
                    buffer[valread] = '\0';
                    printf("Message from client %d: %s\n", sd, buffer);

                    // Broadcast to other clients
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        int out_sd = client_sockets[j];
                        if (out_sd != 0 && out_sd != sd) {
                            send(out_sd, buffer, strlen(buffer), 0);
                        }
                    }
                }
            }
        }
    }

    return 0;
}
