
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
void send_file(const char *filename);
int setup_pipes(int pipe_child_to_parent[2]);
pid_t fork_and_exec(int fd, int pipe_child_to_parent[2]);
void transfer_with_transformation(int pipe_child_to_parent[2]);
void transfer_without_transformation(int file);
int validate_ip(const char *ip_str);
int validate_port(int port);
int update_server_address(int *server_socket, struct sockaddr_in6 *addr, const char *new_ip, int new_port); 

int get_command_value( char* command ){

    if (strncmp(command, "USER", 4) == 0) {
        log_connection(LOGGER_FILE, cli_data->ip, "User command");
        return USER;
    } else if (strncmp(command, "QUIT", 4) == 0) {
        log_connection(LOGGER_FILE, cli_data->ip, "Quit command");
        return QUIT;
    } else if (strncmp(command, "PASS", 4) == 0) {
        log_connection(LOGGER_FILE, cli_data->ip, "Pass command");
        return PASS;
    } else if ( strncmp(command, "STAT", 4) == 0 ){
        log_connection(LOGGER_FILE, cli_data->ip, "Stat command"); 
        return STAT;
    } else if ( strncmp(command, "LIST", 4) == 0 ){ 
        log_connection(LOGGER_FILE, cli_data->ip, "List command");
        return LIST;
    } else if ( strncmp(command, "RETR", 4) == 0 ){ 
        log_connection(LOGGER_FILE, cli_data->ip, "Retr command");
        return RETR;
    } else if ( strncmp(command, "DELE", 4) == 0 ){ 
        log_connection(LOGGER_FILE, cli_data->ip, "Dele command");
        return DELE;
    } else if ( strncmp(command, "NOOP", 4) == 0 ){
        log_connection(LOGGER_FILE, cli_data->ip, "Noop command");
        return NOOP;
    } else if ( strncmp(command, "RSET", 4) == 0 ){
        log_connection(LOGGER_FILE, cli_data->ip, "Rset command");
        return RSET;
    } else if ( strncmp(command, "INFO", 4) == 0 ){
        log_connection(LOGGER_FILE, cli_data->ip, "Info command");
        return INFO;
    } else if ( strncmp(command, "METR", 4) == 0 ){
        log_connection(LOGGER_FILE, cli_data->ip, "Metr command");
        return METR;
    } else if ( strncmp(command, "LOGG", 4) == 0 ){
        log_connection(LOGGER_FILE, cli_data->ip, "Logg command");
        return LOGG;
    } else if (strncmp(command, "PORT", 4) == 0) {
        log_connection(LOGGER_FILE, cli_data->ip, "Port command");
        return PORT;
    } else if (strncmp(command, "HOST", 4) == 0) {
        log_connection(LOGGER_FILE, cli_data->ip, "Host command");
        return HOST;
    } else if (strncmp(command, "DIRR", 4) == 0) {
        log_connection(LOGGER_FILE, cli_data->ip, "Dirr command");
        return DIRR;
    } else if (strncmp(command, "TRAN", 4) == 0) {
        log_connection(LOGGER_FILE, cli_data->ip, "Tran command");
        return TRAN;
    } else if (strncmp(command, "IPV6", 4) == 0) {
        log_connection(LOGGER_FILE, cli_data->ip, "Ipv6 command");
        return IPV6;
    } else {
        log_connection(LOGGER_FILE, cli_data->ip, "Error command");
        return ERROR_COMMAND;
    }
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
    
    // leo del socket
    ssize_t bytes_read = recv(socket_fd, recv_buffer, buffer_size - 1, 0);
    if (bytes_read > 0) {
        recv_buffer[bytes_read] = '\0';

    } else if (bytes_read == 0) {
        fprintf(stderr, "Connection closed by peer.\n");
        cli_data->client_state = CLOSING;
        return 0;
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return -1;
    } else {
        perror("recv failed");
        return -2;
    }
    
    return bytes_read;
}

int write_socket_buffer(char *send_buffer, int socket_fd, const char *data, size_t data_len) {
    size_t total_sent = 0;
    ssize_t bytes_sent = 0;

    while (total_sent < data_len) {
        size_t remaining_data = data_len - total_sent;
        size_t to_copy = remaining_data < BUFFER_SIZE ? remaining_data : BUFFER_SIZE;
        memcpy(send_buffer, data + total_sent, to_copy);

        bytes_sent = send(socket_fd, send_buffer, to_copy, 0);

        if (bytes_sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                perror("send failed");
                return -1;
            }
        }

        total_sent += bytes_sent;
    }

    return total_sent;
}

int stat_handeler(){
    
    int total_bytes = 0;
    file_list* aux = cli_data->maildir->new->list;
    while( aux != NULL ){
        total_bytes += aux->size;
        aux = aux->next;
    }
    aux = cli_data->maildir->cur->list;
    while( aux != NULL ){
        total_bytes += aux->size;
        aux = aux->next;
    }
    aux = cli_data->maildir->tmp->list;
    while( aux != NULL ){
        total_bytes += aux->size;
        aux = aux->next;
    }
    int total_messages = cli_data->maildir->new->size + cli_data->maildir->cur->size + cli_data->maildir->tmp->size;

    char* response = calloc(1, sizeof(char)*BUFFER_SIZE);
    if ( response == NULL ){
        return 1;
    }
    sprintf(response, "+OK %d messages (%d octets)\r\n", total_messages, total_bytes);
    write_socket_buffer(cli_data->send_buffer, cli_data->cli_socket, response, strlen(response));
    // send(cli_socket, response, strlen(response), 0);
    free(response);
    return 0;
}

void list_directory(file_list_header* header, int* index, char* buffer) {
    int bytes_written = 0;
    file_list* current = header->list;
    while (current != NULL) {
        bytes_written = snprintf(buffer, BUFFER_SIZE, "%d %d\r\n", (*index)++, current->size);
        write_socket_buffer(cli_data->send_buffer, cli_data->cli_socket, buffer, bytes_written);
        // send(cli_socket, buffer, bytes_written, 0);
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
        list_directory(cli_data->maildir->new, &index, buffer);
        list_directory(cli_data->maildir->cur, &index, buffer);
        list_directory(cli_data->maildir->tmp, &index, buffer);

        // Send end of list marker
        write_socket_buffer(cli_data->send_buffer, cli_data->cli_socket, ".\r\n", 3);
        //sendcli_socket, ".\r\n", 3, 0);
    } 
    else if (message_number > 0) { 
        int remaining = message_number;
        file_list* current = NULL;

        if (remaining <= cli_data->maildir->new->size) { 
            current = cli_data->maildir->new->list;
        } else if (remaining <= cli_data->maildir->new->size + cli_data->maildir->cur->size) {
            remaining -= cli_data->maildir->new->size;
            current = cli_data->maildir->cur->list;
        } else if (remaining <= cli_data->maildir->new->size + cli_data->maildir->cur->size + cli_data->maildir->tmp->size) {
            remaining -= (cli_data->maildir->new->size + cli_data->maildir->cur->size);
            current = cli_data->maildir->tmp->list;
        } else {
            return -1; // Message number is out of range
        }

        while (--remaining && current != NULL) {
            current = current->next;
        }

        if (current) { 
            bytes_written = snprintf(buffer, BUFFER_SIZE, "%d %d\r\n", message_number, current->size);
            write_socket_buffer(cli_data->send_buffer, cli_data->cli_socket, buffer, bytes_written);
            //sendcli_socket, buffer, bytes_written, 0);
        } else {
            return -1;
        }
    } else {
        return -1;
    }

    return 0;
}

void log_connection(const char *filename, const char *ip, const char * message) {
    FILE *logfile = fopen(filename, "a");
    if (logfile == NULL) {
        perror("Error opening log file");
        exit(EXIT_FAILURE);
    }

    time_t now = time(NULL);
    char *timestamp = ctime(&now);
    timestamp[strcspn(timestamp, "\n")] = '\0';

    fprintf(logfile, "(%s)->[%s] %s\n", message, timestamp, ip);

    fclose(logfile);
}

void send_file(const char *filename) {
    int file = open(filename, O_RDONLY);
    if (file < 0) {
        perror("Failed to open file");
        return;
    }

    if (cli_data->pop3->trans_enabled && cli_data->client_state == TRANSACTION) {
        int pipe_child_to_parent[2];

        if (setup_pipes(pipe_child_to_parent) != 0) {
            close(file);
            return;
        }

        cli_data->trans_in = -1;
        cli_data->trans_out = pipe_child_to_parent[1];

        pid_t pid = fork_and_exec(file, pipe_child_to_parent);
        if (pid < 0) {
            close(file);
            return;
        }

        if (pid > 0) { 
            transfer_with_transformation(pipe_child_to_parent);
        }
    } else {
        transfer_without_transformation(file);
    }

    write_socket_buffer(cli_data->send_buffer, cli_data->cli_socket, "\r\n.\r\n", 5);

    close(file);
    printf("File transfer complete.\n");
}

void send_file_process(const char *filename) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("Failed to fork for send_file");
        return;
    }

    if (pid == 0) { 
        send_file(filename); 
        exit(EXIT_SUCCESS); 
    } else {
        printf("Started file transfer process with PID: %d\n", pid);
    }
}

int setup_pipes(int pipe_child_to_parent[2]) {
    if (pipe(pipe_child_to_parent) == -1) {
        perror("Failed to create pipes");
        return -1;
    }
    return 0;
}

pid_t fork_and_exec(int fd, int pipe_child_to_parent[2]) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("Failed to fork");
        return -1;
    }

    if (pid == 0) {
        if (dup2(fd, STDIN_FILENO) == -1) {
            perror("Failed to redirect stdin");
            exit(EXIT_FAILURE);
        }
        if (dup2(pipe_child_to_parent[1], STDOUT_FILENO) == -1) {
            perror("Failed to redirect stdout");
            exit(EXIT_FAILURE);
        }

        close(pipe_child_to_parent[1]);

        char *argv[] = {cli_data->pop3->trans->trans_binary_path, cli_data->pop3->trans->trans_args, NULL};
        execve(argv[0], argv, NULL);

        perror("Failed to execute transformation process");
        exit(EXIT_FAILURE);
    }

    close(pipe_child_to_parent[1]); 
    return pid;
}

void transfer_with_transformation( int pipe_child_to_parent[2]) {
    ssize_t bytes_sent, aux_read;

        while ((aux_read = read(pipe_child_to_parent[0], cli_data->send_trans_buffer, sizeof(cli_data->send_trans_buffer))) > 0) {
            if (aux_read < 0) {
                perror("Failed to read from pipe");
                break;
            }
            bytes_sent = write_socket_buffer(cli_data->send_buffer, cli_data->cli_socket, cli_data->send_trans_buffer, aux_read);
            
            if (bytes_sent < 0) {
                perror("Failed to send file data through socket");
                break;
            }
        }

}

void transfer_without_transformation(int file) {
    ssize_t bytes_read, bytes_sent;
    char buffer[BUFFER_SIZE];
    while ((bytes_read = read(file, buffer, sizeof(buffer))) > 0) {
        bytes_sent = write_socket_buffer(cli_data->send_buffer, cli_data->cli_socket, buffer, bytes_read);
        if (bytes_sent < 0) {
            perror("Failed to send file data through socket");
            break;
        }
    }

    if (bytes_read < 0) {
        perror("Failed to read file");
    }
}

void check_if_ready_output(){
    ssize_t bytes_read, bytes_sent;
    if ( !cli_data->pop3->trans_enabled )
        return;

    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(cli_data->trans_out, &writefds);
    int activity = select(cli_data->trans_out + 1,NULL, &writefds, NULL, NULL);
    if (activity < 0) {
        perror("Error en select");
        exit(1);
    }

    if ( FD_ISSET(cli_data->trans_out, &writefds) ){
        bytes_read = read( cli_data->trans_out, cli_data->send_trans_buffer, sizeof(BUFFER_SIZE) );
        if (bytes_read < 0) {
            return;
        }else if (bytes_read == 0){
            write_socket_buffer(cli_data->send_buffer, cli_data->cli_socket, "\n\r", 2);
            return;
        }
        bytes_sent = write_socket_buffer(cli_data->send_buffer, cli_data->cli_socket, cli_data->send_trans_buffer, bytes_read);
        if (bytes_sent < 0) {
            perror("Failed to send file data");
            return;
        }
    }

    return;
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

int view_message( int file_number, Metrics* metrics ){

    // reccoro la lista buscando el archivo y obtengo el nombre con el numero
    char buffer[BUFFER_SIZE];
    file_list * file = search_file( cli_data->maildir->new, file_number );
    int dir = -1;
    if ( file == NULL ){
        file = search_file( cli_data->maildir->cur, file_number );
        if ( file == NULL ){
            file = search_file( cli_data->maildir->tmp, file_number );
            if ( file == NULL ){
                char* response = "-ERR Message not found\r\n";
                write_socket_buffer(cli_data->send_buffer, cli_data->cli_socket, response, strlen(response));
                //sendcli_socket, response, strlen(response), 0);
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
    metrics->total_bytes += file->size;
    metrics->total_messages++;
    write_socket_buffer(cli_data->send_buffer, cli_data->cli_socket, buffer2, strlen(buffer2));
    //sendcli_socket, buffer2, strlen(buffer2), 0);

    sprintf(buffer, "%s%s/%s/%s", pop3->base_dir, cli_data->user->name, dir == NEW ? "new" : dir == CUR ? "cur" : "tmp", file->name);

    send_file_process( buffer );
    return 0;
}

void unmark_all(){
    file_list* aux = cli_data->maildir->new->list;
    while ( aux != NULL ){
        if ( aux->deleted ){
            aux->deleted = false;
        }else{
            aux = aux->next;
        }
    }

    aux = cli_data->maildir->cur->list;
    while ( aux != NULL ){
        if ( aux->deleted ){
            aux->deleted = false;
        }else{
            aux = aux->next;
        }
    }

    aux = cli_data->maildir->tmp->list;
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
    file_list* aux = cli_data->maildir->new->list;
    int i=0;
    while ( aux != NULL ){
        if ( aux->deleted ){
            sprintf(path, "%s%s/%s/%s", pop3->base_dir, cli_data->user->name, "new", aux->name);
            if (remove( path ) == -1){
                perror("Error al borrar archivo");
            }
            // libero la memoria de la lista del archivo
            file_list* next = aux->next;
            if ( i == 0 ){
                cli_data->maildir->new->list = aux->next;
            }
            free(aux);
            aux = next;

            cli_data->maildir->new->size--;
        }else{
            i++;
            aux = aux->next;
        }
    }

    // reseteo
    i=0;
    aux = cli_data->maildir->cur->list;
    while ( aux != NULL ){
        if ( aux->deleted ){
            sprintf(path, "%s%s/%s/%s", pop3->base_dir, cli_data->user->name, "cur", aux->name);
            if (remove( path ) == -1){
                perror("Error al borrar archivo");
            }
            // libero la memoria de la lista del archivo
            file_list* next = aux->next;
            if ( i == 0 ){
                cli_data->maildir->cur->list = aux->next;
            }
            free(aux);
            aux = next;

            cli_data->maildir->cur->size--;
        }else{
            i++;
            aux = aux->next;
        }
    }

    i=0;
    aux = cli_data->maildir->tmp->list;
    while ( aux != NULL ){
        if ( aux->deleted ){
            sprintf(path, "%s%s/%s/%s", pop3->base_dir, cli_data->user->name, "tmp", aux->name);
            if (remove( path ) == -1){
                perror("Error al borrar archivo");
            }
            // libero la memoria de la lista del archivo
            file_list* next = aux->next;
            if ( i == 0 ){
                cli_data->maildir->tmp->list = aux->next;
            }
            free(aux);
            aux = next;

            cli_data->maildir->tmp->size--;
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
        write_socket_buffer(cli_data->send_buffer, cli_data->cli_socket, response, strlen(response));
        //sendcli_socket, response, strlen(response), 0);
        return 1;
    }else if ( file_number > cli_data->maildir->new->size + cli_data->maildir->cur->size + cli_data->maildir->tmp->size ){
        char* response = "-ERR Message not found\r\n";
        write_socket_buffer(cli_data->send_buffer, cli_data->cli_socket, response, strlen(response));
        //sendcli_socket, response, strlen(response), 0);
        return 1;
    }else if ( file_number <= cli_data->maildir->new->size ){
        
        mark_del_in_list( cli_data->maildir->new, file_number);

    }else if ( file_number <= cli_data->maildir->new->size + cli_data->maildir->cur->size ){
        mark_del_in_list( cli_data->maildir->cur, file_number-cli_data->maildir->new->size);

    }else{
        mark_del_in_list( cli_data->maildir->tmp, file_number-(cli_data->maildir->new->size + cli_data->maildir->cur->size));
        
    }

    char* response = malloc(sizeof(char)*BUFFER_SIZE);
    sprintf(response,"+OK message %d deleted\r\n", file_number);
    write_socket_buffer(cli_data->send_buffer, cli_data->cli_socket, response, strlen(response));
    //sendcli_socket, response, strlen(response), 0);

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

    sprintf(user_path, "%s%s", pop3->base_dir, cli_data->user->name);

    char new_path[PATH_MAX];
    char cur_path[PATH_MAX];
    char tmp_path[PATH_MAX];

    snprintf(new_path, sizeof(new_path), "%s/new", user_path);
    snprintf(cur_path, sizeof(cur_path), "%s/cur", user_path);
    snprintf(tmp_path, sizeof(tmp_path), "%s/tmp", user_path);

    cli_data->maildir = calloc(1, sizeof(maildir));

    cli_data->maildir->new = create_file_list(new_path);
    if (!cli_data->maildir->new) {
        fprintf(stderr, "Error al inicializar la carpeta 'new'\n");
        // Continuar aunque falle, dependiendo de la lógica de tu servidor
    }
    cli_data->maildir->new->size = amount_mails(cli_data->maildir->new);

    cli_data->maildir->cur = create_file_list(cur_path);
    if (!cli_data->maildir->cur) {
        fprintf(stderr, "Error al inicializar la carpeta 'cur'\n");
    }
    cli_data->maildir->cur->size = amount_mails(cli_data->maildir->cur);

    cli_data->maildir->tmp = create_file_list(tmp_path);
    if (!cli_data->maildir->tmp) {
        fprintf(stderr, "Error al inicializar la carpeta 'tmp'\n");
    }
    cli_data->maildir->tmp->size = amount_mails(cli_data->maildir->tmp);

    return 0;
}

int destroy_user_list(file_list_header* list){
    file_list* aux = list->list;
    while ( aux != NULL ){
        file_list* next = aux->next;
        free(aux->name);
        free(aux);
        aux = next;
    }
    free(list);
    return 0;
}

int destroy_maildir(){

    destroy_user_list(cli_data->maildir->cur);
    destroy_user_list(cli_data->maildir->new);
    destroy_user_list(cli_data->maildir->tmp);
    free(cli_data->maildir);
    return 0;

}

int parse_number(buffer* buff){

    char* number = buffer_read_ptr( buff, &(size_t){ buff->write-buff->read } );
    int num = atoi(number);
    buffer_read_adv( buff, buff->write-buff->read );

    return num;
}

void handle_client(Client_data* client_data, Metrics* metrics, int* server_socket, struct sockaddr_in6* server_addr, int* server_fd) {
    ssize_t bytes_received;
    
    pop3 = client_data->pop3;
    cli_data = client_data;

    char* response;
    char buff[BUFFER_SIZE];
    char command[BUFFER_SIZE*2];
    strcpy(command, client_data->recv_buffer);
    client_data->recv_buffer[0] = '\0';

    bytes_received = read_socket_buffer(buff, client_data->cli_socket, BUFFER_SIZE);

    // checkeo si se desconecto o hubo un error
    if (bytes_received <= 0) {
        // Error o desconexión del cliente
        if (bytes_received == 0) {
            printf("Client disconnected.\n");
        } else {
            perror("recv error");
        }
    }
    char* aux = buff;
    char* aux_init=aux;
    // confirmo que tengo un comando
    while( *aux != '\0' ){
        if ( *aux == '\n' ){
            if ( aux != buff && *(aux-1) == '\r'){
                *(aux-1) = '\0';
            }
            *aux = '\0';
            strcat(command, aux_init);

            buffer *b = malloc(sizeof(buffer));
            buffer_init(b, BUFFER_SIZE*2, command);
            
            buffer_write_adv(b, strlen(command));
            //char* aux = buffer_read_ptr( b, &(size_t){ 4 } );
            int comm = get_command_value(command);
            buffer_read_adv( b, (size_t) 4 );

            switch(comm){
                case USER:
                    if ( client_data->client_state != AUTHORIZATION && client_data->client_state != MANAGER){
                        client_data->client_state = AUTHORIZATION;
                    }
                    if ( buffer_read( b ) == '\0' ){
                        response = "-ERR Missing username\r\n";
                        write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                        //sendcli_socket, response, strlen(response), 0);
                    } else {
                        if ( client_validation( b, client_data ) ){
                            response = "-ERR User not found\r\n";
                            write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                            //sendcli_socket, response, strlen(response), 0);
                        } else {
                            response = "+OK User accepted, password needed\r\n";
                            write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                            //sendcli_socket, response, strlen(response), 0);
                        }
                    }
                    break;
                case PASS:
                    if ( client_data->client_state != AUTHORIZATION ){
                        response = "-ERR You had already logged on\r\n";
                        write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                        //sendcli_socket, response, strlen(response), 0);
                        break;
                    }
                    if ( buffer_read( b ) == '\0' ){
                        response = "-ERR Missing password\r\n";
                        write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                        //sendcli_socket, response, strlen(response), 0);
                        break;
                    } else {
                        if ( password_validation( b, client_data ) ){
                            response = "-ERR Password incorrect\r\n";
                            write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                            //sendcli_socket, response, strlen(response), 0);
                        } else {
                            response = "+OK Password accepted\r\n";
                            write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                            //sendcli_socket, response, strlen(response), 0);
                            client_data->client_state = TRANSACTION;
                            if ( load_user_structure() ){
                                response = "-ERR Error loading user structure\r\n";
                                write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                                //sendcli_socket, response, strlen(response), 0);
                            }
                        }
                    }
                    break;
                case RSET:
                    if ( client_data->client_state != TRANSACTION ){
                        response = "-ERR You must log in first\r\n";
                        write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                        //sendcli_socket, response, strlen(response), 0);
                        break;
                    }
                    unmark_all();
                    break;
                case STAT:
                    if ( client_data->client_state != TRANSACTION ){
                        response = "-ERR You must log in first\r\n";
                        write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                        //sendcli_socket, response, strlen(response), 0);
                        break;
                    }
                    stat_handeler();
                    break;
                case LIST:
                    if ( client_data->client_state != TRANSACTION ){
                        response = "-ERR You must log in first\r\n";
                        write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                        //sendcli_socket, response, strlen(response), 0);
                        break;
                    }
                    if ( buffer_read( b ) == '\0' ){
                        list_messages(-1);
                        break;
                    } else {
                        int num = parse_number( b );
                        if ( num == 0 ){
                            response = "-ERR Argument Error \r\n";
                            write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                            //sendcli_socket, response, strlen(response), 0);
                            break;
                        }
                        list_messages(num);
                    }
                    break;
                case RETR:
                    if ( client_data->client_state != TRANSACTION ){
                        response = "-ERR You must log in first\r\n";
                        write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                        //sendcli_socket, response, strlen(response), 0);
                        break;
                    }
                    if ( buffer_read( b ) == '\0' ){
                        response = "-ERR Argument Error \r\n";
                        write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                        //sendcli_socket, response, strlen(response), 0);
                        break;
                    } else {
                        // parseo el numero 
                        int num = parse_number( b );
                        if ( num == 0 ){
                            response = "-ERR Argument Error \r\n";
                            write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                            //sendcli_socket, response, strlen(response), 0);
                            break;
                        }
                        view_message(num, metrics);
                        
                    }
                    break;
                case DELE:
                    if ( client_data->client_state != TRANSACTION ){
                        response = "-ERR You must log in first\r\n";
                        write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                        //sendcli_socket, response, strlen(response), 0);
                        break;
                    }
                    if ( buffer_read( b ) == '\0' ){
                        response = "-ERR Argument Error \r\n";
                        write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                        //sendcli_socket, response, strlen(response), 0);
                        break;
                    } else {
                        // parseo el numero 
                        int num = parse_number( b );
                        if ( num == 0 ){
                            response = "-ERR Argument Error \r\n";
                            write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                            //sendcli_socket, response, strlen(response), 0);
                            break;
                        }
                        // marca como borrado el mensaje
                        delete_message(num);
                    }
                    break;
                case NOOP:
                    // Retorna un OK!, no hace nada
                    response = "+OK\r\n";
                    write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                    //sendcli_socket, response, strlen(response), 0);
                    break;
                case PORT:
                    if ( client_data->client_state != MANAGER ){
                        response = "-ERR You lack privillege\r\n";
                        write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                        break;
                    }
                    response = "+OK\r\n";
                    write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                    *server_fd = update_server_address(server_socket, server_addr, NULL, atoi(command+5));
                    break;
                case HOST:
                    if ( client_data->client_state != MANAGER ){
                        response = "-ERR You lack privillege\r\n";
                        write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                        break;
                    }
                    char * host = calloc(1, strlen(command+5));
                    strcpy(host, command+5);

                    pop3->host = host;

                    response = "+OK\r\n";
                    write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response)); 
                    break;
                case DIRR:
                    if ( client_data->client_state != MANAGER ){
                        response = "-ERR You lack privillege\r\n";
                        write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                        break;
                    }
                    char * new_dir = calloc(1, strlen(command+5));
                    strcpy(new_dir, command+5);

                    pop3->base_dir = new_dir;

                    response = "+OK\r\n";
                    write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                    break;
                case TRAN:
                    if ( client_data->client_state != MANAGER ){
                        response = "-ERR You lack privillege\r\n";
                        write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                        break;
                    }
                    pop3->trans_enabled = atoi(command+5) == 1 ? true : false;
                    response = "+OK\r\n";
                    write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                    break;
                case IPV6:
                    if ( client_data->client_state != MANAGER ){
                        response = "-ERR You lack privillege\r\n";
                        write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                        break;
                    }
                    response = "+OK\r\n";
                    char * ip = calloc(1, strlen(command+5)+1);
                    strcpy(ip, command+5);
                    write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                    *server_fd = update_server_address(server_socket, server_addr, ip, 0);
                    free(ip);
                    break;
                case INFO:
                    if ( client_data->client_state != MANAGER ){
                        response = "-ERR You lack privillege\r\n";
                        write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                        break;
                    }
                    char * info = calloc(1, BUFFER_SIZE);
                    sprintf(info, "+OK %s;%s;%s\r\n", pop3->host, pop3->base_dir, pop3->ip);
                    write_socket_buffer(client_data->send_buffer, client_data->cli_socket, info, strlen(info));
                    free(info);
                    break;
                case METR:
                    if ( client_data->client_state != MANAGER ){
                        response = "-ERR You lack privillege\r\n";
                        write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                        break;
                    }
                    char * metr = calloc(1, BUFFER_SIZE);
                    sprintf(metr, "+OK %d;%d;%d;%d\r\n", metrics->total_messages, metrics->total_bytes, metrics->total_historic_connections, metrics->max_consecutive_connections);
                    write_socket_buffer(client_data->send_buffer, client_data->cli_socket, metr, strlen(metr));
                    free(metr);
                    break;
                case LOGG:
                    if ( client_data->client_state != MANAGER ){
                        response = "-ERR You lack privillege\r\n";
                        write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                        break;
                    }
                    response = "+OK Log command\r\n";
                    write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                    send_file_process(LOGGER_FILE);
                    break;
                case QUIT:
                    response = "+OK Goodbye\r\n";
                    write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                    //sendcli_socket, response, strlen(response), 0);
                    if ( client_data->client_state == TRANSACTION ){
                        delete_marked();
                        destroy_maildir();
                    }
                    client_data->client_state = CLOSING;
                    printf("Client disconnected.\n");
                    close(client_data->cli_socket);
                    free(b);
                    return;
                default:
                    response = "-ERR Unknown command\r\n";
                    write_socket_buffer(client_data->send_buffer, client_data->cli_socket, response, strlen(response));
                    buffer_compact(b);
            }
            free(b);
            command[0] = '\0';

            aux_init = aux+1;
        }
        aux=aux+1;
    }
    if ( aux_init != aux ){
        strcat(client_data->recv_buffer, aux_init);
    }


    

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

int validate_ip(const char *ip_str) {
    struct sockaddr_in sa;
    struct sockaddr_in6 sa6;

    if (inet_pton(AF_INET, ip_str, &(sa.sin_addr)) == 1) {
        return 1;
    }

    if (inet_pton(AF_INET6, ip_str, &(sa6.sin6_addr)) == 1) {
        return 1;
    }

    return 0;
}

int validate_port(int port) {
    return (port >= MIN_PORT && port <= MAX_PORT);
}

int update_server_address(int *server_socket, struct sockaddr_in6 *addr, const char *new_ip, int new_port) {
    if (new_ip == NULL && new_port > 0) {
        addr->sin6_port = htons(new_port);
        printf("Port updated to: %d\n", new_port);
    }
    else if (new_ip != NULL && new_port == 0) {
        if (inet_pton(AF_INET6, new_ip, &(addr->sin6_addr)) <= 0) {
            perror("Invalid IPv6 address");
            return -1;
        }
        printf("IP address updated to: %s\n", new_ip);
    } 
    else {
        printf("Cannot update both IP and Port at the same time.\n");
        return -1;
    }

    int* aux_socket;

     *aux_socket = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (*aux_socket == -1) {
        perror("Socket creation failed");
        return -1;
    }

    int opt = 1;
    if (setsockopt(*aux_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(*aux_socket);
        return -1;
    }

    opt = 0;
    if (setsockopt(*aux_socket, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(IPV6_V6ONLY) failed");
        close(*aux_socket);
        return -1;
    }

    if (bind(*aux_socket, (struct sockaddr*) addr, sizeof(*addr)) < 0) {
        perror("Bind failed");
        close(*aux_socket);
        return -1;
    }

    if (listen(*aux_socket, 5) < 0) {
        perror("Listen failed");
        close(*aux_socket);
        return -1;
    }


    close(*server_socket);
    *server_socket = *aux_socket;

    return *server_socket;
}

int password_validation(buffer* buff, Client_data* client_data) {

    if ( client_data->user == NULL || client_data->user->pass == NULL ){
        return 1;
    }
    
    char* pass = buffer_read_ptr( buff, &(size_t){ buff->write-buff->read } );

    size_t pass_len = strlen(client_data->user->pass);
    // Compara la contraseña leída con la almacenada
    if (pass_len == strlen(pass)) {
        while (pass_len-- > 0) {
            if (client_data->user->pass[pass_len] != pass[pass_len]) {
                buffer_read_adv( buff, buff->write-buff->read );
                return 1;  // Contraseña incorrecta
            }
        }
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
        free(aux->name);
        free(aux->pass);
        free(aux);
        aux = next;
    }
    if(pop3_struct->trans_enabled){
        if(pop3_struct->trans->trans_binary_path != NULL)
            free(pop3_struct->trans->trans_binary_path);
        if(pop3_struct->trans->trans_args != NULL)
            free(pop3_struct->trans->trans_args);
    }
    free(pop3_struct->trans);
    free(pop3_struct->user_list);
    free(pop3_struct);
}
