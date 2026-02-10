#define KTERM_IMPLEMENTATION
#include "../kterm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define SERVER_PORT 9090
#define BUFFER_SIZE 1024

// Global server state
int server_fd = -1;
int client_fd = -1;

// Output callback: Send terminal output to the connected client
void server_output_callback(KTerm* term, const char* data, int length) {
    if (client_fd >= 0) {
        // In a real server, you'd buffer this to handle partial writes
        // For this example, we just write and ignore errors/blocking (bad practice for prod)
        // But since we are in non-blocking mode, we should handle EAGAIN
        ssize_t sent = send(client_fd, data, length, 0);
        if (sent < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("send failed");
                close(client_fd);
                client_fd = -1;
            }
        }
    }
}

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    printf("Starting KTerm TCP Server on port %d...\n", SERVER_PORT);

    // 1. Initialize KTerm
    KTermConfig config = {0};
    config.width = 80;
    config.height = 24;
    config.response_callback = server_output_callback;

    KTerm* term = KTerm_Create(config);
    if (!term) {
        fprintf(stderr, "Failed to create KTerm instance\n");
        return 1;
    }

    // 2. Setup Server Socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    set_nonblocking(server_fd);

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(server_fd, 1) < 0) {
        perror("listen");
        return 1;
    }

    printf("Listening... Connect with 'nc localhost %d'\n", SERVER_PORT);

    // 3. Main Loop
    fd_set readfds;
    char buffer[BUFFER_SIZE];

    // Initial welcome message
    KTerm_QueueResponse(term, "Welcome to KTerm Server!\r\n");

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_fd = server_fd;

        if (client_fd >= 0) {
            FD_SET(client_fd, &readfds);
            if (client_fd > max_fd) max_fd = client_fd;
        }

        struct timeval timeout = {0, 10000}; // 10ms timeout for UI update loop integration
        int activity = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

        if (activity < 0 && errno != EINTR) {
            perror("select");
            break;
        }

        // Check for new connection
        if (FD_ISSET(server_fd, &readfds)) {
            int new_socket = accept(server_fd, NULL, NULL);
            if (new_socket >= 0) {
                if (client_fd >= 0) {
                    // Reject if already connected (simple 1-client server)
                    const char* msg = "Server busy. Try again later.\r\n";
                    send(new_socket, msg, strlen(msg), 0);
                    close(new_socket);
                } else {
                    client_fd = new_socket;
                    set_nonblocking(client_fd);
                    printf("Client connected!\n");

                    // Force a redraw / welcome prompt
                    const char* prompt = "\r\n$ ";
                    KTerm_QueueResponse(term, prompt);
                }
            }
        }

        // Check for client data
        if (client_fd >= 0 && FD_ISSET(client_fd, &readfds)) {
            ssize_t valread = read(client_fd, buffer, BUFFER_SIZE);
            if (valread > 0) {
                // Pipe network data into KTerm input
                // KTerm_ProcessEventsInternal handles raw bytes injection
                // But typically we use KTerm_ProcessChar or similar for injection.
                // KTerm_ProcessChar injects into input pipeline (keys/mouse)
                // Wait, if this is a remote terminal, the bytes coming in ARE the keys.
                // So we inject them into the active session.

                KTermSession* s = GET_SESSION(term);
                for (int i = 0; i < valread; i++) {
                    KTerm_ProcessChar(term, s, (unsigned char)buffer[i]);
                }
            } else if (valread == 0) {
                // Disconnected
                printf("Client disconnected.\n");
                close(client_fd);
                client_fd = -1;
            } else {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("read");
                    close(client_fd);
                    client_fd = -1;
                }
            }
        }

        // Process Terminal Updates
        // KTerm_Update processes queued events and generates response callbacks if needed (echo)
        KTerm_Update(term);
    }

    KTerm_Destroy(term);
    close(server_fd);
    return 0;
}
