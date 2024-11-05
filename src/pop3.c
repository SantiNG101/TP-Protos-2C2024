
#include "pop3.h"
#include <unistd.h>


static char user_path[BUFFER_SIZE];

// Private Functions
int get_command_value( char* command ){


    if (strncmp(command, "USER", 4) == 0) {
        return USER;
    } else if (strncmp(command, "QUIT", 4) == 0) {
        return QUIT;
    } else if (strncmp(command, "PASS", 4) == 0) {
        return PASS;
    } else if ( strncmp(command, "STAT", 4) == 0 ){ 
        return STAT;
    } else if ( strncmp(command, "LIST", 4) == 0 ){ 
        return LIST;
    } else if ( strncmp(command, "RETR", 4) == 0 ){ 
        return RETR;
    } else if ( strncmp(command, "DELE", 4) == 0 ){ 
        return DELE;
    } else if ( strncmp(command, "NOOP", 4) == 0 ){
        return NOOP;
    }else
        return ERROR_COMMAND;

}



void handle_client(int client_socket) {
    char buffer1[BUFFER_SIZE];
    ssize_t bytes_received;
    buffer *b = malloc(sizeof(buffer));
    buffer_init(b, BUFFER_SIZE, buffer1);

    send(client_socket, "+OK POP3 server ready\r\n", 23, 0);

    while (1) {
        char* response;
        bytes_received = recv(client_socket, buffer1, BUFFER_SIZE, 0);
        // checkeo si se desconecto o hubo un error
        if (bytes_received <= 0) {
            // Error o desconexión del cliente
            if (bytes_received == 0) {
                printf("Client disconnected.\n");
            } else {
                perror("recv error");
            }
            break;
        }
        buffer_compact(b); // reinicio el buffer para que no haya problemas
        buffer_write_adv(b, bytes_received-1); // -1 porque recibe el \n
        buffer_write(b, '\0'); 
        char* aux = buffer_read_ptr( b, &(size_t){ 4 } );
        buffer_read_adv( b, (size_t) 4 );

        switch(get_command_value(aux)){
            case USER:
                if ( buffer_read( b ) == '\0' ){
                    response = "-ERR Missing username\r\n";
                    send(client_socket, response, strlen(response), 0);
                } else {
                    if ( client_validation( b ) ){
                        response = "-ERR User not found\r\n";
                        send(client_socket, response, strlen(response), 0);
                    } else {
                        response = "+OK User accepted, password needed\r\n";
                        send(client_socket, response, strlen(response), 0);
                    }
                }
                break;
            case PASS:
                if ( buffer_read( b ) == '\0' ){
                    response = "-ERR Missing password\r\n";
                    send(client_socket, response, strlen(response), 0);
                    continue;
                } else {
                    if ( password_validation( b ) ){
                        response = "-ERR Password incorrect\r\n";
                        send(client_socket, response, strlen(response), 0);
                    } else {
                        response = "+OK Password accepted\r\n";
                        send(client_socket, response, strlen(response), 0);
                    }
                }
                break;
            case STAT:

            case LIST:

            case RETR:

            case DELE:

            case NOOP:
                // no hace nada, mantiene la conexino abierta
                // ver la condicion de corte, me imagino se tiene que bloquear hasta que le llegue algo
                // Retorna un OK!
                response = "OK!!\r\n";
                send(client_socket, response, strlen(response), 0);
                break;
            case QUIT:
                response = "+OK Goodbye\r\n";
                send(client_socket, response, strlen(response), 0);
                printf("Client disconnected.\n");
                close(client_socket);
                goto end;
            default:
                response = "-ERR Unknown command\r\n";
                send(client_socket, response, strlen(response), 0);
        }
        
        
    }
end:
    free(b);
}

int client_validation(buffer* buff) {
    char* user = buffer_read_ptr( buff, &(size_t){ buff->write-buff->read } ); // leeme todo lo que hay
    buffer_read_adv( buff, buff->write-buff->read );
    sprintf(user_path,"%s%s", BASE_DIR, user);

    struct stat statbuf;

    if (stat(user_path, &statbuf) == 0) {
        if (S_ISDIR(statbuf.st_mode)) {
            printf("El directorio '%s' existe.\n", user_path);
        } else {
            printf("El archivo '%s' existe, pero no es un directorio.\n", user_path);
            return 2;
        }
    } else {
        perror("Error al obtener información del directorio");
        return 1;
    }
    return 0;
}

int password_validation(buffer* buff) {
    char user_path_extended[2048];
    char* pass = buffer_read_ptr( buff, &(size_t){ buff->write-buff->read } );
    pass[buff->write-buff->read-1] = '\0'; // elimino el \n
    buffer_read_adv( buff, buff->write-buff->read );
    sprintf(user_path_extended, "%s%s", user_path, "/pass.txt");

    FILE *file = fopen(user_path_extended, "r");
    if (file == NULL) {
        perror("Error al abrir el archivo");
        return 1;
    }

    char line[256];
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return 1;  // Error al leer la contraseña o archivo vacío
    }

    fclose(file);

    // Elimina el salto de línea al final de `line` si existe
    size_t line_len = strlen(line);
    if (line_len > 0 && line[line_len - 1] == '\n') {
        line[line_len - 1] = '\0';
    }
    size_t pass_len = strlen(pass);

    // Compara la contraseña leída con la almacenada
    if (strncmp(line, pass, pass_len) == 0) {
        return 0;  // Contraseña correcta
    } else {
        return 1;  // Contraseña incorrecta
    }
}