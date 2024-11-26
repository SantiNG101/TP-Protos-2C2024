
#include "../shared/args.h"
#include "pop3.h"


// A 1024 sockets. 1 server 1 cliente -> 512 clientes max en simultaneo
// => 512 eventos maximo en simultaneo 

#define MAX_CLIENTS 1022
#define MAX_EVENTS 1024

static int client_count = 0;
static int historic_client_count = 0;
static bool done = false;


static void
sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}

struct Client_data clients[MAX_CLIENTS];

struct pollfd pollfds[MAX_EVENTS];

void init_pollfds() {
    for (int i = 0; i < MAX_EVENTS; i++) {
        pollfds[i].fd = -1;
        pollfds[i].events = POLLIN;
    }
}

int add_client(int client_fd, pop3_structure* pop3_struct, Metrics* metrics, unsigned char is_manager) {
    if(client_count < MAX_CLIENTS + 1){
        pollfds[client_count+2].fd = client_fd;
        pollfds[client_count+2].events = POLLIN;

        metrics->total_historic_connections++;
        metrics->max_consecutive_connections= client_count > metrics->max_consecutive_connections? client_count : metrics->max_consecutive_connections;

        if(is_manager){
            clients[client_count].client_state = MANAGER;
        } else {
            clients[client_count].client_state = AUTHORIZATION;
        }

        clients[client_count].pop3 = pop3_struct;
        clients[client_count].user = NULL;
        return 0;
    }
    return -1;
}

int handle_close_client(int index) {
    close(pollfds[index].fd);
    pollfds[index].fd = pollfds[client_count+1].fd;
    pollfds[index].events = pollfds[client_count+1].events;

    pollfds[client_count+1].fd = -1;
    pollfds[client_count+1].events = POLLIN;

    clients[index-2] = clients[client_count-1];

    clients[client_count-1].cli_socket = -1;
    clients[client_count-1].client_state = -1;
    clients[client_count-1].user = NULL;

    client_count--;
    return 0;
}

void log_connection(const char *filename, const char *message) {
    FILE *logfile = fopen(filename, "a");
    if (logfile == NULL) {
        perror("Error opening log file");
        exit(EXIT_FAILURE);
    }

    time_t now = time(NULL);
    char *timestamp = ctime(&now);
    timestamp[strcspn(timestamp, "\n")] = '\0';

    fprintf(logfile, "[%s] %s\n", timestamp, message);

    fclose(logfile);
}

void get_client_ip(const struct sockaddr_storage *client_addr, char *ip_str, size_t ip_str_size) {
    if (client_addr->ss_family == AF_INET) {
        // IPv4
        struct sockaddr_in *addr_in = (struct sockaddr_in *)client_addr;
        inet_ntop(AF_INET, &(addr_in->sin_addr), ip_str, ip_str_size);
    } else if (client_addr->ss_family == AF_INET6) {
        // IPv6
        struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)client_addr;
        inet_ntop(AF_INET6, &(addr_in6->sin6_addr), ip_str, ip_str_size);
    } else {
        // Unknown family
        strncpy(ip_str, "Unknown AF", ip_str_size);
    }
}

int main( const int argc, char **argv ) {
    int server_socket;
    int manager_server_socket; 

    struct sockaddr_in6 server_addr;
    struct sockaddr_in6 server_manager_addr;

    pop3_structure* pop3_struct = calloc(1, sizeof(pop3_structure));
    signal(SIGINT, sigterm_handler);
    // arguments
    parse_args(argc, argv, pop3_struct);

    Metrics* metrics = calloc(1, sizeof(Metrics));
    metrics->total_messages = 0;
    metrics->total_bytes = 0;
    metrics->total_historic_connections = 0;
    metrics->max_consecutive_connections = 0;

    server_socket = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    pop3_struct->server_socket = server_socket;

    manager_server_socket = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (manager_server_socket == -1) {
        perror("Manager socket creation failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    pop3_struct->mng_socket = manager_server_socket;

    int opt = 0;
    if (setsockopt(server_socket, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) < 0) {
        perror("Failed to set IPV6_V6ONLY");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (setsockopt(manager_server_socket, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) < 0) {
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

    if (setsockopt(manager_server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_addr = in6addr_any; 
    server_addr.sin6_port = htons(pop3_struct->port);

    memset(&server_manager_addr, 0, sizeof(server_manager_addr));
    server_manager_addr.sin6_family = AF_INET6;
    server_manager_addr.sin6_addr = in6addr_any; 
    server_manager_addr.sin6_port = htons(pop3_struct->mng_port);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Binding failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (bind(manager_server_socket, (struct sockaddr*)&server_manager_addr, sizeof(server_manager_addr)) < 0) {
        perror("Binding of manager socket failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listening failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(manager_server_socket, MAX_CLIENTS) < 0) {
        perror("Listening of manager socket failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server started on port %d and for managers on port %d, accepting IPv4 and IPv6 connections...\n", pop3_struct->port, pop3_struct->mng_port);

    init_pollfds();

    pollfds[0].fd = server_socket;
    pollfds[0].events = POLLIN;
    pollfds[1].fd = manager_server_socket;
    pollfds[1].events = POLLIN;

    while (!done) {
        int poll_count = poll(pollfds, client_count + 2, -1);

        if (poll_count < 0) {
            perror("poll failed");
            break;
        }

        for (int i = 0; i <= client_count+1; i++) {
            if (pollfds[i].revents & POLLIN) {
                pollfds[i].revents = 0;

                if (pollfds[i].fd == server_socket || pollfds[i].fd == manager_server_socket) {
                    struct sockaddr_storage client_addr;
                    socklen_t addr_len = sizeof(client_addr);
                    int client_socket = accept(pollfds[i].fd, (struct sockaddr*)&client_addr, &addr_len);
                    if (client_socket < 0) {
                        perror("Accept failed");
                    } else {
                        if (add_client(client_socket, pop3_struct, metrics, pollfds[i].fd == manager_server_socket) < 0) {
                            printf("Too many clients\n");
                            close(client_socket);
                        } else {
                            char client_ip[INET6_ADDRSTRLEN]; // Buffer for the IP address
                            get_client_ip(&client_addr, client_ip, sizeof(client_ip));

                            printf("Client connected.\n");
                            clients[client_count].cli_socket = client_socket;
                            write_socket_buffer(clients[client_count].send_buffer, clients[client_count].cli_socket, "+OK POP3 server ready\r\n", 23);
                            client_count++;
                            historic_client_count++;
                            log_connection("connections.log", client_ip);
                        }
                    }
                } else {
                    handle_client(&clients[i-2], metrics);
                    if(clients[i-2].client_state == ERROR_CLIENT || clients[i-2].client_state == CLOSING){
                        handle_close_client(i--);
                    }
                }
            }
        }
    }
    
    free(metrics);
    free_pop3_structure(pop3_struct);
    close(server_socket);
    close(manager_server_socket);

    return 0;
}
