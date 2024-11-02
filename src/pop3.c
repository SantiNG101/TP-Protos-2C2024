
#include "pop3.h"
#include <unistd.h>


char* user_path;

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
            if ( buffer[4] == '\0' ){
                send(client_socket, "-ERR Missing username\r\n", 23, 0);
                continue;
            } else {
                int num = client_validation(buffer + 5);
                printf("Num: %d\n", num);
                if ( num ){
                    send(client_socket, "-ERR User not found\r\n", 21, 0);
                } else {
                    send(client_socket, "+OK User accepted\r\n", 19, 0);
                }
            }
        } else if (strncmp(buffer, "QUIT", 4) == 0) {
            send(client_socket, "+OK Goodbye\r\n", 13, 0);
            printf("Client disconnected.\n");
            close(client_socket);
            break;
        } else {
            send(client_socket, "-ERR Unknown command\r\n", 22, 0);
        }
    }
}

int client_validation(char* buffer) {
    char* user = read_user(buffer);
    user_path = calloc(1 , 256*sizeof(char) );

    sprintf(user_path,"%s%s", BASE_DIR, user);

    //printf("User: %s\n", user);
    printf("Path: %s\n", user_path);

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



char* read_user(char* buffer) {
    char* user = calloc(1,256*sizeof(char));
    int i = 0;
    while (buffer[i] != '\n') {
        user[i] = buffer[i];
        i++;
    }
    user[i]='\0';
    return user;
}