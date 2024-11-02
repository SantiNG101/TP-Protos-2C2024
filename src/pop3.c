
#include "pop3.h"
#include <unistd.h>


char* user_path;

void handle_client(int client_socket) {
    char buffer1[BUFFER_SIZE];
    ssize_t bytes_received;
    buffer *b = malloc(sizeof(buffer));
    buffer_init(b, BUFFER_SIZE, buffer1);

    send(client_socket, "+OK POP3 server ready\r\n", 23, 0);

    while (1) {
        bytes_received = recv(client_socket, buffer1, BUFFER_SIZE, 0);
        buffer_write_adv(b, bytes_received-1); // -1 porque recibe el \n
        /* ya tira el de arriba un assert
        if (bytes_received <= 0) {
            printf("Client disconnected.\n");
            close(client_socket);
            break;
        }*/

        //buffer1[bytes_received-1] = '\0';
        buffer_write(b, '\0');

        char* aux = buffer_read_ptr( b, &(size_t){ 4 } );
        buffer_read_adv( b, (size_t) 4 );

        if (strncmp(aux, "USER", 4) == 0) {
            if ( buffer_read( b ) == '\0' ){
                send(client_socket, "-ERR Missing username\r\n", 23, 0);
                continue;
            } else {
                if ( client_validation( b ) ){
                    send(client_socket, "-ERR User not found\r\n", 21, 0);
                } else {
                    send(client_socket, "+OK User accepted, password needed\r\n", 36, 0);
                }
            }
        } else if (strncmp(aux, "QUIT", 4) == 0) {
            send(client_socket, "+OK Goodbye\r\n", 13, 0);
            printf("Client disconnected.\n");
            close(client_socket);
            break;
        } else if (strncmp(aux, "PASS", 4) == 0) {
            if ( buffer_read( b ) == '\0' ){
                send(client_socket, "-ERR Missing password\r\n", 23, 0);
                continue;
            } else {
                if ( password_validation( b ) ){
                    send(client_socket, "-ERR Password incorrect\r\n", 25, 0);
                } else {
                    send(client_socket, "+OK Password accepted\r\n", 25, 0);
                }
            }
        } else {
            send(client_socket, "-ERR Unknown command\r\n", 22, 0);
        }
        buffer_compact(b); // reinicio el buffer para que no haya problemas
    }

    free(b);
}

int client_validation(buffer* buff) {
    char* user = buffer_read_ptr( buff, &(size_t){ 256 } );
    buffer_read_adv( buff, (size_t)256 );
    user_path = calloc(1 , 1024*sizeof(char) );

    sprintf(user_path,"%s%s", BASE_DIR, user);

    struct stat statbuf;

    if (stat(user_path, &statbuf) == 0) {
        if (S_ISDIR(statbuf.st_mode)) {
            printf("El directorio '%s' existe.\n", user_path);
        } else {
            printf("El archivo '%s' existe, pero no es un directorio.\n", user_path);
            free(user);
            return 2;
        }
    } else {
        perror("Error al obtener información del directorio");
        free(user);
        return 1;
    }

    free(user);
    return 0;
}


/*
char* read_user(char* buffer1) {
    char* user = calloc(1,256*sizeof(char));
    int i = 0;
    while (buffer1[i] != '\n' && buffer1[i] != '\0') {
        user[i] = buffer1[i];
        i++;
    }
    user[i]='\0';
    return user;
}*/

int password_validation(buffer* buff) {
    char user_path_extended[256];
    char* pass = buffer_read_ptr( buff, &(size_t){ 256 } );
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