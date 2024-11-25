
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

int add_client(int client_fd, pop3_structure* pop3_struct) {
    if(client_count < MAX_CLIENTS + 1){
        pollfds[client_count+1].fd = client_fd;
        pollfds[client_count+1].events = POLLIN;

        clients[client_count].client_state = AUTHORIZATION;
        clients[client_count].pop3 = pop3_struct;
        clients[client_count].user = NULL;
        return 0;
    }
    return -1;
}

int handle_close_client(int index) {
    close(pollfds[index].fd);
    pollfds[index].fd = pollfds[client_count].fd;
    pollfds[index].events = pollfds[client_count].events;

    pollfds[client_count].fd = -1;
    pollfds[client_count].events = POLLIN;

    clients[index-1] = clients[client_count-1];

    clients[client_count-1].cli_socket = -1;
    clients[client_count-1].client_state = -1;
    clients[client_count-1].user = NULL;

    client_count--;
    return 0;
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

    opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
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

    // creo el programa de traduccion
    if ( pop3_struct->trans_enabled ){
        int fd[2];
        // Crear el pipe
        if (pipe(fd) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }

        pop3_struct->trans->trans_in = fd[0];
        pop3_struct->trans->trans_out = fd[1];

        // Crear un nuevo proceso
        int pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) {
            // proceso hijo
            // Cerrar el descriptor de lectura
            close(0);
            close(1);
            // Redirigir la salida estandar al pipe
            dup2(fd[0], STDIN_FILENO);
            dup2(fd[1], STDOUT_FILENO);

            close(fd[0]);
            close(fd[1]);

            // Ejecutar el programa de traduccion
            execve(pop3_struct->trans->trans_binary_path, &pop3_struct->trans->trans_args, NULL);
            perror("execve");
            exit(EXIT_FAILURE);
        }

    }


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
                        if (add_client(client_socket, pop3_struct) < 0) {
                            printf("Too many clients\n");
                            close(client_socket);
                        } else {
                            printf("Client connected.\n");
                            clients[client_count].cli_socket = client_socket;
                            write_socket_buffer(clients[client_count].send_buffer, clients[client_count].cli_socket, "+OK POP3 server ready\r\n", 23);
                            client_count++;
                        }
                    }
                } else {
                    handle_client(&clients[i-1]);
                    if(clients[i-1].client_state == ERROR_CLIENT || clients[i-1].client_state == CLOSING){
                        handle_close_client(i--);
                    }
                }
            }
        }
    }

    free_pop3_structure(pop3_struct);
    free(pop3_struct);
    close(server_socket);

    return 0;
}
