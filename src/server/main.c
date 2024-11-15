
#include "../shared/args.h"
#include "pop3.h"


// A 1024 sockets. 1 server 1 cliente -> 512 clientes max en simultaneo
// => 512 eventos maximo en simultaneo 

#define MAX_CLIENTS 512
#define MAX_EVENTS 512

static int client_count = 0;
static bool done = false;


static void
sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}

struct Client_data clients[MAX_CLIENTS];

struct pollfd pollfds[MAX_CLIENTS + 1];

void init_pollfds() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        pollfds[i].fd = -1;
        pollfds[i].events = POLLIN;
    }
}

void init_clientData( pop3_structure* pop3_struct ) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].client_state = AUTHORIZATION;
        clients[i].pop3 = pop3_struct;
        clients[i].user = calloc(1, sizeof(User));
    }
}

int add_client(int client_fd) {
    for (int i = 1; i <= MAX_CLIENTS; i++) {
        if (pollfds[i].fd == -1) {
            pollfds[i].fd = client_fd;
            pollfds[i].events = POLLIN;
            return 0;
        }
    }
    return -1;
}

int main( const int argc, char **argv ) {
    int server_socket;
    struct sockaddr_in6 server_addr;
    pop3_structure* pop3_struct = calloc(1, sizeof(pop3_structure));

    // arguments
    parse_args(argc, argv, pop3_struct);

    server_socket = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    pop3_struct->server_socket = server_socket;

    int opt = 0;
    if (setsockopt(server_socket, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) < 0) {
        perror("Failed to set IPV6_V6ONLY");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_addr = in6addr_any; 
    server_addr.sin6_port = htons(pop3_struct->port);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Binding failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listening failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    printf("Server started on port %d, accepting IPv4 and IPv6 connections...\n", pop3_struct->port);

    init_pollfds();
    init_clientData(pop3_struct);

    pollfds[0].fd = server_socket;
    pollfds[0].events = POLLIN;

    while (!done) {
        int poll_count = poll(pollfds, client_count + 1, -1);

        if (poll_count < 0) {
            perror("poll failed");
            break;
        }

        for (int i = 0; i <= client_count; i++) {
            if (pollfds[i].revents & POLLIN) {
                pollfds[i].revents = 0;

                if (pollfds[i].fd == server_socket) {
                    struct sockaddr_storage client_addr;
                    socklen_t addr_len = sizeof(client_addr);
                    int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len);
                    if (client_socket < 0) {
                        perror("Accept failed");
                    } else {
                        if (add_client(client_socket) < 0) {
                            printf("Too many clients\n");
                            close(client_socket);
                        } else {
                            client_count++;
                            printf("Client connected.\n");
                        }
                    }
                } else {
                    clients[i].pop3->cli_socket = pollfds[i].fd;
                    // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, "+OK POP3 server ready\r\n", 23);
                    send(clients[i].pop3->cli_socket, "+OK POP3 server ready\r\n", 23, 0);
                    handle_client(&clients[i]);
                }
            }
        }
    }

    free_pop3_structure(pop3_struct);
    free(pop3_struct);
    close(server_socket);

    return 0;
}
