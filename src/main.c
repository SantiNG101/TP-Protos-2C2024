
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>

#include <unistd.h>
#include <sys/types.h>   // socket
#include <sys/socket.h>  // socket
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define PORT 1110
#define BUFFER_SIZE 1024
#include "selector.h"

/*
Estructura de la carpeta para el argumento -d:
$ tree .
.
└── user1
    ├── cur
    ├── new
    │   └── mail1
    └── tmp
*/
static void
usage(const char *progname) {
    fprintf(stderr,
            "Usage: %s [OPTION]...\n"
            "\n"
            "   -h               Imprime la ayuda y termina.\n"
            "   -l <POP3 addr>   Dirección donde servirá el servidor POP.\n"
            "   -L <conf  addr>  Dirección donde servirá el servicio de management.\n"
            "   -p <POP3 port>   Puerto entrante conexiones POP3.\n"
            "   -P <conf port>   Puerto entrante conexiones configuracion\n"
            "   -u <name>:<pass> Usuario y contraseña de usuario que puede usar el servidor. Hasta 10.\n"
            "   -v               Imprime información sobre la versión versión y termina.\n"
            "   -d               Carpeta donde residen los Maildirs"
            "\n",
            progname);
    exit(1);
}

static bool done = false;

static void
sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}

void handle_client(int client_socket);

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Binding failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) < 0) {
        perror("Listening failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    printf("POP3 Server started on port %d...\n", PORT);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_size);
        if (client_socket < 0) {
            perror("Client connection failed");
            continue;
        }
        printf("Client connected.\n");

        handle_client(client_socket);
    }

    close(server_socket);
    return 0;
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    send(client_socket, "+OK POP3 server ready\r\n", 23, 0);

    while (1) {
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            printf("Client disconnected.\n");
            close(client_socket);
            break;
        }

        buffer[bytes_received] = '\0';

        if (strncmp(buffer, "USER", 4) == 0) {
            send(client_socket, "+OK User accepted\r\n", 19, 0);
        } else if (strncmp(buffer, "QUIT", 4) == 0) {
            send(client_socket, "+OK Goodbye\r\n", 13, 0);
            close(client_socket);
            break;
        } else {
            send(client_socket, "-ERR Unknown command\r\n", 22, 0);
        }
    }
}
