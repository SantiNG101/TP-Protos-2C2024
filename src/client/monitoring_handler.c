#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "./header/monitoring_handler.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define BUFFER_SIZE 256

#define LINE_END "\r\n"
#define CLIENT_IDENTIFIER "pop3_monitor"
#define AUTH_USER_CMD "USER user1" LINE_END
#define AUTH_PASS_CMD "PASS pass1" LINE_END
#define STATUS_CMD "STAT" LINE_END
#define TERMINATE_CMD "QUIT" LINE_END

#define ONLINE 1
#define OFFLINE 0

#define NOT_REACHABLE "Unreachable"
#define IS_REACHABLE "Online"

// Códigos de error personalizados
#define SOCKET_ERROR -1
#define AUTH_ERROR -2
#define STAT_ERROR -3

int connection_status = OFFLINE;
static int pop3_socket = -1;
double response_time_ms = 0.0;
int total_messages = 0;
int total_bytes = 0;
int online_connections = 0;
int total_connections =0;

char response_buffer[BUFFER_SIZE] = {'\0'};

static int establish_socket_connection(char *address, char *port_number) {
    struct addrinfo hints, *result, *rp;
    int socket_fd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_STREAM;   

    int ret = getaddrinfo(address, port_number, &hints, &result);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return SOCKET_ERROR;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (socket_fd == -1)
            continue;  

        if (connect(socket_fd, rp->ai_addr, rp->ai_addrlen) != -1) {
            break;  
        }

        close(socket_fd); 
    }

    freeaddrinfo(result);

    if (rp == NULL) {
        fprintf(stderr, "Could not connect to any address\n");
        return SOCKET_ERROR;
    }

    return socket_fd;
}

int connect_to_pop3_server(char *address, char *port_number) {
    pop3_socket = establish_socket_connection(address, port_number);
    if (pop3_socket == SOCKET_ERROR || recv_initial_response() != 0) {
        return SOCKET_ERROR;
    }

    // Proceso de autenticación
    if (send_command(AUTH_USER_CMD) == 1 || send_command(AUTH_PASS_CMD) == 1) {
        perror("Authentication failed");
        return AUTH_ERROR;
    }

    // Llamada para obtener estadísticas del servidor
    if (retrieve_pop3_stats() != 0) { // Manejo de errores al obtener estadísticas
        perror("Failed to retrieve POP3 statistics");
        return STAT_ERROR;
    }

    connection_status = ONLINE;
    return 0;
}

int recv_initial_response(void) {
    int bytes_received = recv(pop3_socket, response_buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        perror("Failed to receive server greeting");
        return SOCKET_ERROR;
    }
    response_buffer[bytes_received] = '\0';
    printf("Server greeting: %s\n", response_buffer);
    return 0;
}

void disconnect_from_server(void) {
    if (pop3_socket != -1) {
        send_command(TERMINATE_CMD);
        close(pop3_socket);
        pop3_socket = -1; // Resetear el socket después de cerrar
    }
}

int send_command(char* command) {
    if (pop3_socket == -1) {
        connection_status = OFFLINE;
        perror("Socket is not connected");
        return 1; // Indica error en el envío
    }

    size_t command_length = strlen(command);
    if (send(pop3_socket, command, command_length, 0) < 0) {
        connection_status = OFFLINE;
        perror("Failed to send command");
        return 1; // Indica error en el envío
    }

    int bytes_received = recv(pop3_socket, response_buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        connection_status = OFFLINE;
        perror("Failed to receive response");
        return 1; // Indica error en la recepción
    }

    response_buffer[bytes_received] = '\0';
    return 0;
}

void verify_server_status(void) {
    clock_t start_time = clock();

    // Envío un NOOP para chequear el estado del servidor
    if (send_command("NOOP" LINE_END) == 1) {
        connection_status = OFFLINE;
    } else {
        connection_status = ONLINE;
    }

    clock_t end_time = clock();

    response_time_ms = ((double)(end_time - start_time) * 1000) / CLOCKS_PER_SEC;
}

int retrieve_pop3_stats(void) {
    char *token;

    // Enviar el comando STAT para obtener el número total de mensajes y su tamaño
    if (send_command(STATUS_CMD) == 0) {

        // Procesar respuesta utilizando sscanf
        if (sscanf(response_buffer, "+OK %d %d", &total_messages, &total_bytes) != 2) {
            perror("Failed to parse STAT response");
            return STAT_ERROR; // Indica error al analizar la respuesta
        }

        token = strtok(response_buffer, " "); // Ignorar "+OK"
        token = strtok(NULL, " "); // Obtener número de mensajes
        if (token != NULL) {
            total_messages = atoi(token);
        }
        token = strtok(NULL, " "); // Obtener tamaño total
        if (token != NULL) {
            total_bytes = atoi(token);
        }

        return 0;
    } else {
        perror("Failed to retrieve POP3 statistics");
        return STAT_ERROR; // Indica error al recuperar estadísticas
    }
}

char* get_connection_status(void) {
    return connection_status == ONLINE ? IS_REACHABLE : NOT_REACHABLE;
}

double get_response_time(void) {
    return response_time_ms;
}

int get_total_messages(void) {
    return total_messages;
}

int get_total_bytes(void) {
    return total_bytes;
}

int get_online_connections(void){
    return online_connections;
}

int get_total_connections(void){
    return total_connections;
}