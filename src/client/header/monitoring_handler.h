#ifndef MONITORING_HANDLER_H
#define MONITORING_HANDLER_H

// Definiciones de constantes
#define BUFFER_SIZE 256
#define ONLINE 1
#define OFFLINE 0

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