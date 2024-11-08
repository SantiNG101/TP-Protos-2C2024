
#include "pop3.h"
#include <unistd.h>


typedef struct file_list {
    char* name;
    int size;
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

char* nombre_prueba = "Pago_banco_Marta.txt";
char* contenido_prueba = "Date: 9/12/2018\nFrom: Emisor <juliana@bancomarta.com>\nTo: Receptor <receptor@gmail.com>\nSubject: Asunto del Correo\n\nBuenos dias les envio este correo para poder informarles que su cuota del banco Marta esta inpaga por favor acceda a este link para poder regularizar su situacion: https://www.youtube.com/watch?v=dQw4w9WgXcQ";


maildir* user_structure = NULL;
user_list* active_user;
user_list_header* global_user_list;
int cli_socket;
char* base_dir = "./src/root/";

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

int amount_mails( file_list_header* list ){
    int i=0;
    file_list* aux = list->list;
    while ( aux != NULL ){
        i++;
        aux = aux->next;
    }
    return i;
}



int stat_handeler(){
    
    int total_bytes = 0;
    file_list* aux = user_structure->new->list;
    while( aux != NULL ){
        total_bytes += aux->size;
        aux = aux->next;
    }
    aux = user_structure->cur->list;
    while( aux != NULL ){
        total_bytes += aux->size;
        aux = aux->next;
    }
    aux = user_structure->tmp->list;
    while( aux != NULL ){
        total_bytes += aux->size;
        aux = aux->next;
    }
    int total_messages = user_structure->new->size + user_structure->cur->size + user_structure->tmp->size;

    char* response = calloc(1, sizeof(char)*BUFFER_SIZE);
    if ( response == NULL ){
        return 1;
    }
    sprintf(response, "+OK %d %d\r\n", total_messages, total_bytes);
    send(cli_socket, response, strlen(response), 0);
    free(response);
    return 0;
}



void list_directory(file_list_header* header, int* index, char* buffer) {
    int bytes_written = 0;
    file_list* current = header->list;
    while (current != NULL) {
        bytes_written = snprintf(buffer, BUFFER_SIZE, "%d %d\r\n", (*index)++, current->size);
        send(cli_socket, buffer, bytes_written, 0);
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
        list_directory(user_structure->new, &index, buffer);
        list_directory(user_structure->cur, &index, buffer);
        list_directory(user_structure->tmp, &index, buffer);

        // Send end of list marker
        send(cli_socket, ".\r\n", 3, 0);
    } 
    else if (message_number > 0) { 
        int remaining = message_number;
        file_list* current = NULL;

        if (remaining <= user_structure->new->size) { 
            current = user_structure->new->list;
        } else if (remaining <= user_structure->new->size + user_structure->cur->size) {
            remaining -= user_structure->new->size;
            current = user_structure->cur->list;
        } else if (remaining <= user_structure->new->size + user_structure->cur->size + user_structure->tmp->size) {
            remaining -= (user_structure->new->size + user_structure->cur->size);
            current = user_structure->tmp->list;
        } else {
            return -1; // Message number is out of range
        }

        while (--remaining && current != NULL) {
            current = current->next;
        }

        if (current) { 
            bytes_written = snprintf(buffer, BUFFER_SIZE, "%d %d\r\n", message_number, current->size);
            send(cli_socket, buffer, bytes_written, 0);
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
        bytes_sent = send(cli_socket, buffer, bytes_read, 0);
        if (bytes_sent < 0) {
            perror("Failed to send file data");
            break;
        }
    }

    if (bytes_read < 0) {
        perror("Failed to read file");
    }

    close(file);
    printf("File transfer complete.\n");
}

char* search_file( file_list_header* header, int file_number ){
    int remaining = file_number;
    file_list* current = header->list;

    while ( current != NULL ){
        if ( --remaining == 0 ){
            return current->name;
        }
        current = current->next;
    }
    return NULL;
}


int view_message( int file_number ){

    // reccoro la lista buscando el archivo y obtengo el nombre con el numero
    char * name = search_file( user_structure->new, file_number );
    if ( name == NULL ){
        name = search_file( user_structure->cur, file_number );
        if ( name == NULL ){
            name = search_file( user_structure->tmp, file_number );
            if ( name == NULL ){
                char* response = "-ERR Message not found\r\n";
                send(cli_socket, response, strlen(response), 0);
                return 1;
            }
        }
    }

    send_file( name );
    return 0;
}

int delete_message(){
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

    sprintf(user_path, "%s%s/", base_dir, active_user->name);

    char new_path[PATH_MAX];
    char cur_path[PATH_MAX];
    char tmp_path[PATH_MAX];

    snprintf(new_path, sizeof(new_path), "%s/new", user_path);
    snprintf(cur_path, sizeof(cur_path), "%s/cur", user_path);
    snprintf(tmp_path, sizeof(tmp_path), "%s/tmp", user_path);

    user_structure = calloc(1, sizeof(maildir));

    user_structure->new = create_file_list(new_path);
    if (!user_structure->new) {
        fprintf(stderr, "Error al inicializar la carpeta 'new'\n");
        // Continuar aunque falle, dependiendo de la lógica de tu servidor
    }
    user_structure->new->size = amount_mails(user_structure->new);

    user_structure->cur = create_file_list(cur_path);
    if (!user_structure->cur) {
        fprintf(stderr, "Error al inicializar la carpeta 'cur'\n");
    }
    user_structure->cur->size = amount_mails(user_structure->cur);

    user_structure->tmp = create_file_list(tmp_path);
    if (!user_structure->tmp) {
        fprintf(stderr, "Error al inicializar la carpeta 'tmp'\n");
    }
    user_structure->tmp->size = amount_mails(user_structure->tmp);

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

int destroy_user_structure(){

    destroy_user_list(user_structure->cur);
    destroy_user_list(user_structure->new);
    destroy_user_list(user_structure->tmp);
    free(user_structure);
    return 0;

}

void parse_number_for_list(buffer* buff){

    // lo dejo asi y confio; posible error
    char* number = buffer_read_ptr( buff, &(size_t){ buff->write-buff->read } );
    buffer_read_adv( buff, buff->write-buff->read );

    int num = atoi(number);
    if ( num != 0 ){
        list_messages(num);
    }else{
        char* response = "-ERR Error in list arguments\r\n";
        send(cli_socket, response, strlen(response), 0);
    }
}


void handle_client(int client_socket, user_list_header* user_list ) {
    char buffer1[BUFFER_SIZE];
    ssize_t bytes_received;
    buffer *b = malloc(sizeof(buffer));
    buffer_init(b, BUFFER_SIZE, buffer1);

    // asigno las variables globales
    cli_socket = client_socket;
    global_user_list = user_list;

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

        // dependiendo si se tiene \n => 2 o \r\n => 3
        if ( bytes_received < 2 ){
            continue;
        }
        // -2 por \r\n ;; -1 por el \n
        buffer_write_adv(b, bytes_received-1);
        buffer_write(b, '\0'); 
        char* aux = buffer_read_ptr( b, &(size_t){ 4 } );
        buffer_read_adv( b, (size_t) 4 );

        int command = get_command_value(aux);

        if ( command != USER && active_user == NULL ){
            response = "-ERR Please USER command first\r\n";
            send(client_socket, response, strlen(response), 0);
            continue;
        }

        switch(command){
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
                        if ( load_user_structure() ){
                            response = "-ERR Error loading user structure\r\n";
                            send(client_socket, response, strlen(response), 0);
                        }
                    }
                }
                break;
            case STAT:
                stat_handeler();
                break;
            case LIST:
                if ( buffer_read( b ) == '\0' ){
                    list_messages(-1);
                    continue;
                } else {
                    parse_number_for_list( b );
                }
                break;
            case RETR:
                view_message(1);
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
                destroy_user_structure();
                printf("Client disconnected.\n");
                close(client_socket);
                goto end;
            default:
                response = "-ERR Unknown command\r\n";
                send(client_socket, response, strlen(response), 0);
                buffer_compact(b);
        }
        buffer_compact(b); // reinicio el buffer para que no haya problemas
        
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