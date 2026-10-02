#include "coro.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define CHAT_PORT 7777
#define MAX_CHAT_CLIENTS 32
#define BUFFER_SIZE 256

// Tracking connected clients
static int client_fds[MAX_CHAT_CLIENTS];
static int client_count = 0;

// Helper function: sets a socket file descriptor to Non-Blocking mode
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Broadcasts 'msg' to all connected clients EXCEPT the sender_fd
static void broadcast(int sender_fd, const char *msg) {
    for (int i = 0; i < client_count; i++) {
        if (client_fds[i] != sender_fd) {
            write(client_fds[i], msg, strlen(msg));
        }
    }
}

// Removes a disconnected client from the client_fds array
static void remove_client(int client_fd) {
    for (int i = 0; i < client_count; i++) {
        if (client_fds[i] == client_fd) {
            // Swap with the last client in the array and decrement count
            client_fds[i] = client_fds[client_count - 1];
            client_count--;
            break;
        }
    }
}

// Each connected chatter runs in their own Coroutine!
void chat_worker(void *arg) {
    int client_fd = (int)(intptr_t)arg;

    // Register this client in the broadcast list
    if (client_count < MAX_CHAT_CLIENTS) {
        client_fds[client_count++] = client_fd;
    }

    char banner[128];
    snprintf(banner, sizeof(banner), "=== Welcome to Coroutine Chat (You are User %d) ===\nType anything to broadcast, or 'quit' to exit.\n\n", client_fd);
    write(client_fd, banner, strlen(banner));

    // Announce to everyone else that a new user joined!
    char join_msg[128];
    snprintf(join_msg, sizeof(join_msg), "[+] User %d joined the chat room!\n", client_fd);
    printf("%s", join_msg);
    broadcast(client_fd, join_msg);

    char buffer[BUFFER_SIZE];

    while (true) {
        // Non-blocking read from client socket
        ssize_t n = read(client_fd, buffer, sizeof(buffer) - 1);

        if (n > 0) {
            buffer[n] = '\0';

            // Clean up trailing \r or \n
            while (n > 0 && (buffer[n - 1] == '\n' || buffer[n - 1] == '\r')) {
                buffer[--n] = '\0';
            }

            if (n == 0) continue; // Ignore empty lines

            // If user typed 'quit', exit chat
            if (strcasecmp(buffer, "quit") == 0) {
                const char *bye = "Goodbye!\n";
                write(client_fd, bye, strlen(bye));
                break;
            }

            // Format message and broadcast to all other chatters!
            char chat_msg[BUFFER_SIZE + 64];
            snprintf(chat_msg, sizeof(chat_msg), "[User %d]: %s\n", client_fd, buffer);
            printf("%s", chat_msg);
            broadcast(client_fd, chat_msg);

        } else if (n == 0) {
            // Client closed connection (Ctrl+C or EOF)
            break;
        } else {
            // Error handling for non-blocking read
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // NO DATA READY RIGHT NOW!
                // Voluntarily yield the CPU to let another chatter or the listener run!
                co_yield();
            } else {
                // Real socket error
                break;
            }
        }
    }

    // Client leaving cleanup
    remove_client(client_fd);
    close(client_fd);

    char leave_msg[128];
    snprintf(leave_msg, sizeof(leave_msg), "[-] User %d left the chat room.\n", client_fd);
    printf("%s", leave_msg);
    broadcast(-1, leave_msg); // Broadcast to all remaining users
}

// The accept loop also runs as a cooperative Coroutine!
void listener_worker(void *arg) {
    int server_fd = (int)(intptr_t)arg;

    printf("[Listener Coroutine] Listening for connections on 0.0.0.0:%d ...\n", CHAT_PORT);

    while (true) {
        // Non-blocking accept
        int client_fd = accept(server_fd, NULL, NULL);

        if (client_fd > 0) {
            // A new chatter connected!
            set_nonblocking(client_fd);

            // Spawn a brand new Coroutine for this client (Go style: `go chat_worker(...)`)!
            co_go(chat_worker, (void *)(intptr_t)client_fd);
            printf("[Listener Coroutine] Spawned new coroutine for Client FD: %d\n", client_fd);
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No one is connecting right this millisecond.
                // Tiny 1ms sleep so the idle event loop doesn't burn 100% CPU spinning
                usleep(1000); // 1 millisecond sleep
                co_yield();   // Yield CPU to the client coroutines!
            } else {
                perror("accept error");
                break;
            }
        }
    }
}

int main(void) {
    printf("====================================================\n");
    printf(" Coroutine-Powered Concurrent Chat Server in C      \n");
    printf(" Listening on:  0.0.0.0:%d                          \n", CHAT_PORT);
    printf(" Architecture:  1 Single OS Thread with Coroutines! \n");
    printf(" Connect with:  nc 127.0.0.1 %d                     \n", CHAT_PORT);
    printf("====================================================\n\n");

    // 1. Create standard TCP socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    set_nonblocking(server_fd); // Must be non-blocking!

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(CHAT_PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 2. Initialize our custom coroutine library
    co_init();

    // 3. Spawn the listener coroutine!
    co_go(listener_worker, (void *)(intptr_t)server_fd);

    // 4. Start the coroutine scheduler!
    co_run();

    close(server_fd);
    return 0;
}
