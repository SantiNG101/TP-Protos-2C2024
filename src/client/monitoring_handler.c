#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "monitoring_handler.h"

#define BUFFER_SIZE 256

#define LINE_END "\r\n"
#define CLIENT_IDENTIFIER "pop3_monitor"
#define AUTH_USER_CMD "USER your_username" LINE_END
#define AUTH_PASS_CMD "PASS your_password" LINE_END
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

char response_buffer[BUFFER_SIZE] = {'\0'};

static int establish_socket_connection(char *address, char *port_number) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("Error creating socket");
        return SOCKET_ERROR;
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    int port_int = atoi(port_number);

    if (inet_pton(AF_INET, address, &server_address.sin_addr) == -1) {
        perror("Invalid address format");
        close(socket_fd);
        return SOCKET_ERROR;
    }
    server_address.sin_port = htons(port_int);
    server_address.sin_family = AF_INET;

    if (connect(socket_fd, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        perror("Connection failed");
        close(socket_fd);
        return SOCKET_ERROR;
    }

    return socket_fd;
}

int connect_to_pop3_server(char *address, char *port_number) {
    pop3_socket = establish_socket_connection(address, port_number);
    if (pop3_socket == SOCKET_ERROR) {
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