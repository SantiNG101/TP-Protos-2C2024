#ifndef MONITORING_HANDLER_H
#define MONITORING_HANDLER_H

#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>


#define LINE_END "\r\n"
#define CLIENT_IDENTIFIER "pop3_monitor"
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


// Funciones para la conexión y desconexión del servidor POP3
int connect_to_pop3_server(char *address, char *port_number);
void disconnect_from_server(void);
int recv_initial_response(void); 
// Funciones para enviar comandos al servidor
int send_command(char* command);

// Funciones para verificar el estado del servidor
void verify_server_status(void);
int get_connection_status(void);
double get_response_time(void);

// Funciones para obtener estadísticas del servidor
int get_total_messages(void);
int get_total_bytes(void);
int get_online_connections(void);
int get_total_connections(void);
int recv_response(char* command);
#endif // MONITORING_HANDLER_H