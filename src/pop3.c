
#include "pop3.h"
#include <unistd.h>


static char user_path[BUFFER_SIZE];

void handle_client(int client_socket) {
    char buffer1[BUFFER_SIZE];
    ssize_t bytes_received;
    buffer *b = malloc(sizeof(buffer));
    buffer_init(b, BUFFER_SIZE, buffer1);

    send(client_socket, "+OK POP3 server ready\r\n", 23, 0);

    while (1) {
        char* response;
        bytes_received = recv(client_socket, buffer1, BUFFER_SIZE, 0);
        buffer_write_adv(b, bytes_received-1); // -1 porque recibe el \n
        
        buffer_write(b, '\0');

        char* aux = buffer_read_ptr( b, &(size_t){ 4 } );
        buffer_read_adv( b, (size_t) 4 );

        if (strncmp(aux, "USER", 4) == 0) {
            if ( buffer_read( b ) == '\0' ){
                response = "-ERR Missing username\r\n";
                send(client_socket, response, strlen(response), 0);
                continue;
            } else {
                if ( client_validation( b ) ){
                    response = "-ERR User not found\r\n";
                    send(client_socket, response, strlen(response), 0);
                } else {
                    response = "+OK User accepted, password needed\r\n";
                    send(client_socket, response, strlen(response), 0);
                }
            }
        } else if (strncmp(aux, "QUIT", 4) == 0) {
            response = "+OK Goodbye\r\n";
            send(client_socket, response, strlen(response), 0);
            printf("Client disconnected.\n");
            close(client_socket);
            break;
        } else if (strncmp(aux, "PASS", 4) == 0) {
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
        } else {
            response = "-ERR Unknown command\r\n";
            send(client_socket, response, strlen(response), 0);
        }
        buffer_compact(b); // reinicio el buffer para que no haya problemas
    }

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
    buffer_read_adv( buff, buff->write-buff->read );
    sprintf(user_path_extended, "%s%s", user_path, "/pass.txt");

    FILE *file = fopen(user_path_extended, "r");
    if (file == NULL) {
        perror("Error al abrir el archivo");
        return 1;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        printf("%s", line);
    }

    fclose(file);

    if (strcmp(line, pass) == 0) {
        return 0;
    } else {
        return 1;
    }
}