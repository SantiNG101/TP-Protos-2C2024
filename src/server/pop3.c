
#include "pop3.h"
#include <unistd.h>


typedef struct file_list {
    char* name;
    char* content;
    struct file_list* next;
} file_list;

typedef struct file_list_header{
    file_list* list;
    int size;
}file_list_header;

typedef struct maildir {
    file_list_header* cur;
    file_list_header* new;
    file_list_header* tmp;
} maildir;

// Global Variables


maildir* user_structure = NULL;
user_list* active_user;
user_list_header* global_user_list;
// Private Functions

int client_validation(buffer* buff);
int password_validation(buffer* buff);

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

file_list_header* read_directory( char* path ){
    return NULL;
}

int statistics(){
    return 0;
}

int list_messages(){
    return 0;
}

int view_message(){
    return 0;
}

int delete_message(){
    return 0;
}

int load_user_structure(){
    char user_cur[BUFFER_SIZE+4];
    char user_new[BUFFER_SIZE+4]; // paths a las distintas secciones de un maildir
    char user_tmp[BUFFER_SIZE+4];

    user_structure = calloc(1,sizeof(maildir));
    user_structure->cur = read_directory( user_cur );
    user_structure->new = read_directory( user_new );
    user_structure->tmp = read_directory( user_tmp );
    
    return 0;
}

int destroy_user_structure(){
    free(user_structure);
    return 0;
}




void handle_client(int client_socket, user_list_header* user_list ) {
    char buffer1[BUFFER_SIZE];
    ssize_t bytes_received;
    buffer *b = malloc(sizeof(buffer));
    buffer_init(b, BUFFER_SIZE, buffer1);

    global_user_list = user_list;

    send(client_socket, "+OK POP3 server ready\r\n", 23, 0);


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
                        if ( load_user_structure() ){
                            response = "-ERR Error loading user structure\r\n";
                            send(client_socket, response, strlen(response), 0);
                        }
                    }
                }
                break;
            case PASS:
                if ( user_structure == NULL ){
                    response = "-ERR User not found, please USER command first!\r\n";
                    send(client_socket, response, strlen(response), 0);
                    break;
                }
                if ( buffer_read( b ) == '\0' ){
                    response = "-ERR Missing password\r\n";
                    send(client_socket, response, strlen(response), 0);
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
                statistics();
                break;
            case LIST:
                list_messages();
                break;
            case RETR:
                view_message();
                break;
            case DELE:
                delete_message();
                break;
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
        
        
    
end:
    free(b);
}

int client_validation(buffer* buff) {
    char* user = buffer_read_ptr( buff, &(size_t){ buff->write-buff->read } ); // leeme todo lo que hay
    buffer_read_adv( buff, buff->write-buff->read );
    
    if ( global_user_list == NULL || global_user_list->size == 0 ){
        return 1;
    }

    user_list* aux = global_user_list->list;
    int amount = global_user_list->size;
    int user_len = strlen(user);

    while ( amount-- ){
        if ( strncmp(aux->name, user, user_len) == 0 ){
            active_user = aux;
            return 0;
        }
        aux = aux->next;
    }
    return 1;

}

int password_validation(buffer* buff) {
    
    char* pass = buffer_read_ptr( buff, &(size_t){ buff->write-buff->read } );
    pass[buff->write-buff->read-1] = '\0'; // elimino el \n
    buffer_read_adv( buff, buff->write-buff->read );

    size_t pass_len = strlen(pass);
    // Compara la contraseña leída con la almacenada
    if (strncmp(active_user->pass, pass, pass_len) == 0) {
        return 0;  // Contraseña correcta
    } else {
        return 1;  // Contraseña incorrecta
    }
}