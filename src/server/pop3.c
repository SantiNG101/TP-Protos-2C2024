
#include "pop3.h"
#include <unistd.h>

// Global Variables

char* nombre_prueba = "Pago_banco_Marta.txt";
char* contenido_prueba = "Date: 9/12/2018\nFrom: Emisor <juliana@bancomarta.com>\nTo: Receptor <receptor@gmail.com>\nSubject: Asunto del Correo\n\nBuenos dias les envio este correo para poder informarles que su cuota del banco Marta esta inpaga por favor acceda a este link para poder regularizar su situacion: https://www.youtube.com/watch?v=dQw4w9WgXcQ";

pop3_structure* pop3;
Client_data* cli_data;


// Private Functions

int client_validation(buffer* buff, Client_data* client_data);
int password_validation(buffer* buff, Client_data* client_data);

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
    }else if ( strncmp(command, "RSET", 4) == 0 ){
        return RSET;
    }else
        return ERROR_COMMAND;

}

int amount_mails( file_list_header* list ){
    int i=0;
    file_list* aux = list->list;
    while ( aux != NULL ){
        i++;
        aux = aux->next;
    }
    return i;
}

int read_socket_buffer(char *recv_buffer, int socket_fd, size_t buffer_size) {
    ssize_t bytes_read = recv(socket_fd, recv_buffer, buffer_size - 1, 0);

    if (bytes_read > 0) {
        recv_buffer[bytes_read] = '\0';
        return bytes_read;
    } else if (bytes_read == 0) {
        fprintf(stderr, "Connection closed by peer.\n");
        return 0;
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return -1;
    } else {
        perror("recv failed");
        return -2;
    }
}

int write_socket_buffer(char *send_buffer, int socket_fd, const char *data, size_t data_len) {
    size_t total_sent = 0;
    ssize_t bytes_sent = 0;

    while (total_sent < data_len) {
        size_t remaining_data = data_len - total_sent;
        ssize_t to_copy = remaining_data < BUFFER_SIZE ? remaining_data : BUFFER_SIZE;

        memcpy(send_buffer, data + total_sent, to_copy);
        while ((bytes_sent = send(socket_fd, send_buffer, to_copy, 0)) < to_copy) {
            if (bytes_sent < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                } else {
                    perror("send failed");
                    return -1;
                }
            }
            to_copy -= bytes_sent;
        }

        total_sent += bytes_sent;
    }

    return total_sent;
}

int stat_handeler(){
    
    int total_bytes = 0;
    file_list* aux = pop3->maildir->new->list;
    while( aux != NULL ){
        total_bytes += aux->size;
        aux = aux->next;
    }
    aux = pop3->maildir->cur->list;
    while( aux != NULL ){
        total_bytes += aux->size;
        aux = aux->next;
    }
    aux = pop3->maildir->tmp->list;
    while( aux != NULL ){
        total_bytes += aux->size;
        aux = aux->next;
    }
    int total_messages = pop3->maildir->new->size + pop3->maildir->cur->size + pop3->maildir->tmp->size;

    char* response = calloc(1, sizeof(char)*BUFFER_SIZE);
    if ( response == NULL ){
        return 1;
    }
    sprintf(response, "+OK %d messags (%d octets)\r\n", total_messages, total_bytes);
    // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
    send(pop3->cli_socket, response, strlen(response), 0);
    free(response);
    return 0;
}



void list_directory(file_list_header* header, int* index, char* buffer) {
    int bytes_written = 0;
    file_list* current = header->list;
    while (current != NULL) {
        bytes_written = snprintf(buffer, BUFFER_SIZE, "%d %d\r\n", (*index)++, current->size);
        // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, buffer, bytes_written);
        send(pop3->cli_socket, buffer, bytes_written, 0);
        current = current->next;
    }
}

int list_messages(int message_number) {
    char buffer[BUFFER_SIZE]; 
    int bytes_written = 0;

    if (message_number == -1) { 
        int index = 1;
        stat_handeler();
        // List all messages in new, cur, and tmp
        list_directory(pop3->maildir->new, &index, buffer);
        list_directory(pop3->maildir->cur, &index, buffer);
        list_directory(pop3->maildir->tmp, &index, buffer);

        // Send end of list marker
        // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, ".\r\n", 3);
        send(pop3->cli_socket, ".\r\n", 3, 0);
    } 
    else if (message_number > 0) { 
        int remaining = message_number;
        file_list* current = NULL;

        if (remaining <= pop3->maildir->new->size) { 
            current = pop3->maildir->new->list;
        } else if (remaining <= pop3->maildir->new->size + pop3->maildir->cur->size) {
            remaining -= pop3->maildir->new->size;
            current = pop3->maildir->cur->list;
        } else if (remaining <= pop3->maildir->new->size + pop3->maildir->cur->size + pop3->maildir->tmp->size) {
            remaining -= (pop3->maildir->new->size + pop3->maildir->cur->size);
            current = pop3->maildir->tmp->list;
        } else {
            return -1; // Message number is out of range
        }

        while (--remaining && current != NULL) {
            current = current->next;
        }

        if (current) { 
            bytes_written = snprintf(buffer, BUFFER_SIZE, "%d %d\r\n", message_number, current->size);
            // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, buffer, bytes_written);
            send(pop3->cli_socket, buffer, bytes_written, 0);
        } else {
            return -1;
        }
    } else {
        return -1;
    }

    return 0;
}

void send_file(const char *filename) {
    int file = open(filename, O_RDONLY);
    if (file < 0) {
        perror("Failed to open file");
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read, bytes_sent;

    while ((bytes_read = read(file, buffer, sizeof(buffer))) > 0) {
        // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, buffer, bytes_read);
        bytes_sent = send(pop3->cli_socket, buffer, bytes_read, 0);
        if (bytes_sent < 0) {
            perror("Failed to send file data");
            break;
        }
    }

    if (bytes_read < 0) {
        perror("Failed to read file");
    }

    // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, "\n\r", 2);
    send(pop3->cli_socket, "\n\r.\n\r", 5, 0);

    close(file);
    printf("File transfer complete.\n");
}

file_list* search_file( file_list_header* header, int file_number ){
    int remaining = file_number;
    file_list* current = header->list;

    while ( current != NULL ){
        if ( --remaining == 0 ){
            return current;
        }
        current = current->next;
    }
    return NULL;
}

enum pop3_directory {
    NEW,
    CUR,
    TMP
};


int view_message( int file_number ){

    // reccoro la lista buscando el archivo y obtengo el nombre con el numero
    char buffer[BUFFER_SIZE];
    file_list * file = search_file( pop3->maildir->new, file_number );
    int dir = -1;
    if ( file == NULL ){
        file = search_file( pop3->maildir->cur, file_number );
        if ( file == NULL ){
            file = search_file( pop3->maildir->tmp, file_number );
            if ( file == NULL ){
                char* response = "-ERR Message not found\r\n";
                // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
                send(pop3->cli_socket, response, strlen(response), 0);
                return 1;
            }else{
                dir = TMP;
            }
        }else{
            dir = CUR;
        }
    }else{
        dir = NEW;
    }

    char buffer2[BUFFER_SIZE];
    sprintf(buffer2, "+OK %d octets\r\n", file->size);
    // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, buffer2, strlen(buffer2));
    send(pop3->cli_socket, buffer2, strlen(buffer2), 0);

    sprintf(buffer, "%s%s/%s/%s", pop3->base_dir, cli_data->user->name, dir == NEW ? "new" : dir == CUR ? "cur" : "tmp", file->name);

    send_file( buffer );
    return 0;
}

void unmark_all(){
    file_list* aux = pop3->maildir->new->list;
    while ( aux != NULL ){
        if ( aux->deleted ){
            aux->deleted = false;
        }else{
            aux = aux->next;
        }
    }

    aux = pop3->maildir->cur->list;
    while ( aux != NULL ){
        if ( aux->deleted ){
            aux->deleted = false;
        }else{
            aux = aux->next;
        }
    }

    aux = pop3->maildir->tmp->list;
    while ( aux != NULL ){
        if ( aux->deleted ){
            aux->deleted = false;
        }else{
            aux = aux->next;
        }
    }

    stat_handeler();

}

void delete_marked(){
    
    char* path = calloc(1, sizeof(char)*BUFFER_SIZE);
    file_list* aux = pop3->maildir->new->list;
    int i=0;
    while ( aux != NULL ){
        if ( aux->deleted ){
            sprintf(path, "%s%s/%s/%s", pop3->base_dir, cli_data->user->name, "new", aux->name);
            if (!remove( path )){
                perror("Error al borrar archivo");
            }
            // libero la memoria de la lista del archivo
            file_list* next = aux->next;
            if ( i == 0 ){
                pop3->maildir->new->list = aux->next;
            }
            free(aux);
            aux = next;

            pop3->maildir->new->size--;
        }else{
            i++;
            aux = aux->next;
        }
    }

    // reseteo
    i=0;
    aux = pop3->maildir->cur->list;
    while ( aux != NULL ){
        if ( aux->deleted ){
            sprintf(path, "%s%s/%s/%s", pop3->base_dir, cli_data->user->name, "cur", aux->name);
            if (!remove( path )){
                perror("Error al borrar archivo");
            }
            // libero la memoria de la lista del archivo
            file_list* next = aux->next;
            if ( i == 0 ){
                pop3->maildir->cur->list = aux->next;
            }
            free(aux);
            aux = next;

            pop3->maildir->cur->size--;
        }else{
            i++;
            aux = aux->next;
        }
    }

    i=0;
    aux = pop3->maildir->tmp->list;
    while ( aux != NULL ){
        if ( aux->deleted ){
            sprintf(path, "%s%s/%s/%s", pop3->base_dir, cli_data->user->name, "tmp", aux->name);
            if (!remove( path )){
                perror("Error al borrar archivo");
            }
            // libero la memoria de la lista del archivo
            file_list* next = aux->next;
            if ( i == 0 ){
                pop3->maildir->tmp->list = aux->next;
            }
            free(aux);
            aux = next;

            pop3->maildir->tmp->size--;
        }else{
            i++;
            aux = aux->next;
        }
    }

    free(path);
}

void mark_del_in_list( file_list_header* header_list, int file_number){

    file_list* aux = header_list->list;
    if ( file_number == 1 ){
        aux->deleted = true;
        return;
    }
    while ( --file_number > 0 ){
        aux = aux->next;
    }
    aux->deleted = true;
}

int delete_message(int file_number){

    if ( file_number < 1 ){
        char* response = "-ERR Invalid message number\r\n";
        // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
        send(pop3->cli_socket, response, strlen(response), 0);
        return 1;
    }else if ( file_number > pop3->maildir->new->size + pop3->maildir->cur->size + pop3->maildir->tmp->size ){
        char* response = "-ERR Message not found\r\n";
        // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
        send(pop3->cli_socket, response, strlen(response), 0);
        return 1;
    }else if ( file_number <= pop3->maildir->new->size ){
        
        mark_del_in_list( pop3->maildir->new, file_number);

    }else if ( file_number <= pop3->maildir->new->size + pop3->maildir->cur->size ){
        mark_del_in_list( pop3->maildir->cur, file_number-pop3->maildir->new->size);

    }else{
        mark_del_in_list( pop3->maildir->tmp, file_number-(pop3->maildir->new->size + pop3->maildir->cur->size));
        
    }

    char* response = malloc(sizeof(char)*BUFFER_SIZE);
    sprintf(response,"+OK message %d deleted\r\n", file_number);
    // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
    send(pop3->cli_socket, response, strlen(response), 0);

    return 0;
}


file_list_header* create_file_list(const char* directory_path) {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    file_list_header* header = calloc(1, sizeof(file_list_header));
    
    if (!header) {
        perror("Error al asignar memoria para file_list_header");
        return NULL;
    }

    dir = opendir(directory_path);
    if (dir == NULL) {
        perror("Error al abrir el directorio");
        free(header);
        return NULL;
    }

    while ((entry = readdir(dir)) != NULL) {
        // Ignorar las entradas '.' y '..'
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construir la ruta completa del archivo
        char filepath[PATH_MAX];
        snprintf(filepath, sizeof(filepath), "%s/%s", directory_path, entry->d_name);

        // Obtener información del archivo
        if (stat(filepath, &file_stat) == -1) {
            perror("Error al obtener información del archivo");
            continue;
        }

        // Solo procesar archivos regulares
        if (S_ISREG(file_stat.st_mode)) {
            // Crear un nuevo nodo para la lista
            file_list* new_file = calloc(1, sizeof(file_list));
            if (!new_file) {
                perror("Error al asignar memoria para file_list");
                continue;
            }

            // Asignar solo el nombre del archivo
            new_file->name = strdup(entry->d_name);
            new_file->size = file_stat.st_size;
            new_file->deleted = false;
            if (!new_file->name) {
                perror("Error al asignar memoria para el nombre del archivo");
                free(new_file);
                continue;
            }

            // Insertar el nuevo archivo al final de la lista
            if (header->list == NULL) {
                header->list = new_file;
            } else {
                file_list* temp = header->list;
                while (temp->next != NULL) {
                    temp = temp->next;
                }
                temp->next = new_file;
            }

            header->size++;
        }
    }

    closedir(dir);
    return header;
}

int load_user_structure(){

    char user_path[BUFFER_SIZE];

    sprintf(user_path, "%s%s/", pop3->base_dir, cli_data->user->name);

    char new_path[PATH_MAX];
    char cur_path[PATH_MAX];
    char tmp_path[PATH_MAX];

    snprintf(new_path, sizeof(new_path), "%s/new", user_path);
    snprintf(cur_path, sizeof(cur_path), "%s/cur", user_path);
    snprintf(tmp_path, sizeof(tmp_path), "%s/tmp", user_path);

    pop3->maildir = calloc(1, sizeof(maildir));

    pop3->maildir->new = create_file_list(new_path);
    if (!pop3->maildir->new) {
        fprintf(stderr, "Error al inicializar la carpeta 'new'\n");
        // Continuar aunque falle, dependiendo de la lógica de tu servidor
    }
    pop3->maildir->new->size = amount_mails(pop3->maildir->new);

    pop3->maildir->cur = create_file_list(cur_path);
    if (!pop3->maildir->cur) {
        fprintf(stderr, "Error al inicializar la carpeta 'cur'\n");
    }
    pop3->maildir->cur->size = amount_mails(pop3->maildir->cur);

    pop3->maildir->tmp = create_file_list(tmp_path);
    if (!pop3->maildir->tmp) {
        fprintf(stderr, "Error al inicializar la carpeta 'tmp'\n");
    }
    pop3->maildir->tmp->size = amount_mails(pop3->maildir->tmp);

    return 0;
}

int destroy_user_list(file_list_header* list){
    file_list* aux = list->list;
    while ( aux != NULL ){
        file_list* next = aux->next;
        free(aux);
        aux = next;
    }
    free(list);
    return 0;
}

int destroy_maildir(){

    destroy_user_list(pop3->maildir->cur);
    destroy_user_list(pop3->maildir->new);
    destroy_user_list(pop3->maildir->tmp);
    free(pop3->maildir);
    return 0;

}

int parse_number(buffer* buff){

    char* number = buffer_read_ptr( buff, &(size_t){ buff->write-buff->read } );
    int num = atoi(number);
    buffer_read_adv( buff, buff->write-buff->read );

    return num;
}



void handle_client(Client_data* client_data ) {
    char buffer1[BUFFER_SIZE];
    ssize_t bytes_received;
    buffer *b = malloc(sizeof(buffer));
    buffer_init(b, BUFFER_SIZE, buffer1);

    pop3 = client_data->pop3;
    cli_data = client_data;

    char* response;
    bytes_received = read_socket_buffer(buffer1, client_data->pop3->cli_socket, BUFFER_SIZE);
    //bytes_received = recv(pop3->cli_socket, buffer1, BUFFER_SIZE, 0);
    // checkeo si se desconecto o hubo un error
    if (bytes_received <= 0) {
        // Error o desconexión del cliente
        if (bytes_received == 0) {
            printf("Client disconnected.\n");
        } else {
            perror("recv error");
        }
    }
    // dependiendo si se tiene \n => 2 o \r\n => 3
    if ( bytes_received < 2 ){
        return;
    }
    // -2 por \r\n ;; -1 por el \n
    buffer_write_adv(b, bytes_received-1);
    buffer_write(b, '\0'); 
    char* aux = buffer_read_ptr( b, &(size_t){ 4 } );
    int command = get_command_value(aux);
    buffer_read_adv( b, (size_t) 4 );
    

    switch(command){
        case USER:
            if ( client_data->client_state != AUTHORIZATION ){
                client_data->client_state = AUTHORIZATION;
            }
            if ( buffer_read( b ) == '\0' ){
                response = "-ERR Missing username\r\n";
                // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
                send(pop3->cli_socket, response, strlen(response), 0);
            } else {
                if ( client_validation( b, client_data ) ){
                    response = "-ERR User not found\r\n";
                    // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
                    send(pop3->cli_socket, response, strlen(response), 0);
                } else {
                    response = "+OK User accepted, password needed\r\n";
                    // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
                    send(pop3->cli_socket, response, strlen(response), 0);
                }
            }
            break;
        case PASS:
            if ( client_data->client_state != AUTHORIZATION ){
                response = "-ERR You had already logged on\r\n";
                // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
                send(pop3->cli_socket, response, strlen(response), 0);
                break;
            }
            if ( buffer_read( b ) == '\0' ){
                response = "-ERR Missing password\r\n";
                // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
                send(pop3->cli_socket, response, strlen(response), 0);
                break;
            } else {
                if ( password_validation( b, client_data ) ){
                    response = "-ERR Password incorrect\r\n";
                    // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
                    send(pop3->cli_socket, response, strlen(response), 0);
                } else {
                    response = "+OK Password accepted\r\n";
                    // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
                    send(pop3->cli_socket, response, strlen(response), 0);
                    client_data->client_state = TRANSACTION;
                    if ( load_user_structure() ){
                        response = "-ERR Error loading user structure\r\n";
                        // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
                        send(pop3->cli_socket, response, strlen(response), 0);
                    }
                }
            }
            break;
        case RSET:
            if ( client_data->client_state != TRANSACTION ){
                response = "-ERR You must log in first\r\n";
                // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
                send(pop3->cli_socket, response, strlen(response), 0);
                break;
            }
            unmark_all();
            break;
        case STAT:
            if ( client_data->client_state != TRANSACTION ){
                response = "-ERR You must log in first\r\n";
                // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
                send(pop3->cli_socket, response, strlen(response), 0);
                break;
            }
            stat_handeler();
            break;
        case LIST:
            if ( client_data->client_state != TRANSACTION ){
                response = "-ERR You must log in first\r\n";
                // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
                send(pop3->cli_socket, response, strlen(response), 0);
                break;
            }
            if ( buffer_read( b ) == '\0' ){
                list_messages(-1);
                break;
            } else {
                int num = parse_number( b );
                if ( num == 0 ){
                    response = "-ERR Argument Error \r\n";
                    // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
                    send(pop3->cli_socket, response, strlen(response), 0);
                    break;
                }
                list_messages(num);
            }
            break;
        case RETR:
            if ( client_data->client_state != TRANSACTION ){
                response = "-ERR You must log in first\r\n";
                // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
                send(pop3->cli_socket, response, strlen(response), 0);
                break;
            }
            if ( buffer_read( b ) == '\0' ){
                response = "-ERR Argument Error \r\n";
                // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
                send(pop3->cli_socket, response, strlen(response), 0);
                break;
            } else {
                // parseo el numero 
                int num = parse_number( b );
                if ( num == 0 ){
                    response = "-ERR Argument Error \r\n";
                    // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
                    send(pop3->cli_socket, response, strlen(response), 0);
                    break;
                }
                view_message(num);
            }
            break;
        case DELE:
            if ( client_data->client_state != TRANSACTION ){
                response = "-ERR You must log in first\r\n";
                // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
                send(pop3->cli_socket, response, strlen(response), 0);
                break;
            }
            if ( buffer_read( b ) == '\0' ){
                response = "-ERR Argument Error \r\n";
                // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
                send(pop3->cli_socket, response, strlen(response), 0);
                break;
            } else {
                // parseo el numero 
                int num = parse_number( b );
                if ( num == 0 ){
                    response = "-ERR Argument Error \r\n";
                    // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
                    send(pop3->cli_socket, response, strlen(response), 0);
                    break;
                }
                // marca como borrado el mensaje
                delete_message(num);
            }
            break;
        case NOOP:
            // Retorna un OK!, no hace nada
            response = "OK!!\r\n";
            // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
            send(pop3->cli_socket, response, strlen(response), 0);
            break;
        case QUIT:
            response = "+OK Goodbye\r\n";
            // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
            send(pop3->cli_socket, response, strlen(response), 0);
            client_data->client_state = CLOSING;
            //delete_marked();
            //destroy_maildir();
            printf("Client disconnected.\n");
            close(pop3->cli_socket);
            goto end;
        default:
            response = "-ERR Unknown command\r\n";
            // write_socket_buffer(client_data->send_buffer, client_data->pop3->cli_socket, response, strlen(response));
            send(pop3->cli_socket, response, strlen(response), 0);
            buffer_compact(b);
    }
    buffer_compact(b); // reinicio el buffer para que no haya problemas
        
    
end:
    free(b);
}

int client_validation(buffer* buff, Client_data* client_data) {
    char* user = buffer_read_ptr( buff, &(size_t){ buff->write-buff->read } ); // leeme todo lo que hay
    
    
    if ( pop3->user_list == NULL || pop3->user_list->size == 0 ){
        buffer_read_adv( buff, buff->write-buff->read );
        return 1;
    }

    user_list* aux = pop3->user_list->list;
    int amount = pop3->user_list->size;
    int user_len = strlen(user);

    while ( amount-- ){
        if ( strncmp(aux->name, user, user_len) == 0 ){
            client_data->user = aux;
            buffer_read_adv( buff, buff->write-buff->read );
            return 0;
        }
        aux = aux->next;
    }
    buffer_read_adv( buff, buff->write-buff->read );
    return 1;

}

int password_validation(buffer* buff, Client_data* client_data) {

    if ( client_data->user == NULL || client_data->user->pass == NULL ){
        return 1;
    }
    
    char* pass = buffer_read_ptr( buff, &(size_t){ buff->write-buff->read } );
    pass[buff->write-buff->read-1] = '\0'; // elimino el \n

    size_t pass_len = strlen(pass);
    // Compara la contraseña leída con la almacenada
    if (strncmp(client_data->user->pass, pass, pass_len) == 0) {
        buffer_read_adv( buff, buff->write-buff->read );
        return 0;  // Contraseña correcta
    } else {
        buffer_read_adv( buff, buff->write-buff->read );
        return 1;  // Contraseña incorrecta
    }
}

void free_pop3_structure( pop3_structure* pop3_struct ){
    user_list* aux = pop3_struct->user_list->list;
    while ( aux != NULL ){
        user_list* next = aux->next;
        free(aux);
        aux = next;
    }
    free(pop3_struct->user_list);
    free(pop3_struct);
}
