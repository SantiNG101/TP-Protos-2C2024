#include "./header/monitoring_handler.h"
#include "../shared/args.h"

int connection_status = OFFLINE;
static int pop3_socket = -1;
double response_time_ms = 0.0;
int total_messages = 0;
int total_bytes = 0;
int online_connections = 0;
int total_connections =0;
int get_command(char* command);

char response_buffer[BUFFER_SIZE] = {'\0'};

static int establish_socket_connection(char *address, char *port_number) {
    struct addrinfo hints, *result, *rp;
    int socket_fd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_STREAM;   

    int ret = getaddrinfo(address, port_number, &hints, &result);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return SOCKET_ERROR;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (socket_fd == -1)
            continue;  

        if (connect(socket_fd, rp->ai_addr, rp->ai_addrlen) != -1) {
            break;  
        }

        close(socket_fd); 
    }

    freeaddrinfo(result);

    if (rp == NULL) {
        fprintf(stderr, "Could not connect to any address\n");
        return SOCKET_ERROR;
    }

    return socket_fd;
}

int connect_to_pop3_server(char *address, char *port_number) {
    pop3_socket = establish_socket_connection(address, port_number);
    if (pop3_socket == SOCKET_ERROR || recv_initial_response() != 0) {
        return SOCKET_ERROR;
    }

    connection_status = ONLINE;
    return 0;
}

int recv_initial_response(void) {
    int bytes_received = recv(pop3_socket, response_buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        perror("Failed to receive server greeting");
        return SOCKET_ERROR;
    }
    response_buffer[bytes_received] = '\0';
    printf("Server greeting: %s\n", response_buffer);
    return 0;
}

void disconnect_from_server(void) {
    if (pop3_socket != -1) {
        send_command(TERMINATE_CMD);
        close(pop3_socket);
        pop3_socket = -1; // Resetear el socket después de cerrar
    }
}

int send_command(char* command) {
    if (pop3_socket == -1) {
        connection_status = OFFLINE;
        perror("Socket is not connected");
        return 1; // Indica error en el envío
    }

    size_t command_length = strlen(command);
    if (send(pop3_socket, command, command_length, 0) < 0) {
        connection_status = OFFLINE;
        perror("Failed to send command");
        return 1; // Indica error en el envío
    }

    return 0;
}

char* extract_token(const char* str, char delim, int* index) {
    int start = *index;
    
    // Find the position of the next delimiter or end of the string
    while (str[*index] != '\0' && str[*index] != delim) {
        (*index)++;
    }

    // Calculate the length of the current token
    int length = *index - start;

    // If no token was found
    if (length == 0 && str[*index] == '\0') {
        return NULL;
    }

    // Allocate memory for the token and copy it
    char* token = calloc(length + 1, sizeof(char));
    if (token == NULL) {
        perror("Memory allocation failed");
        return NULL;
    }
    strncpy(token, str + start, length);

    // Move past the delimiter, if any
    if (str[*index] == delim) {
        (*index)++;
    }

    return token;
}

char** separator(const char* str, char delim, int type) {
    int index = 0;
    int count = (type == METR) ? 4 : 3;

    // Allocate memory for the result array
    char** result = calloc(count + 1, sizeof(char*)); // +1 for NULL terminator
    if (result == NULL) {
        perror("Memory allocation failed");
        return NULL;
    }

    // Extract tokens
    for (int i = 0; i < count; i++) {
        result[i] = extract_token(str, delim, &index);
        if (result[i] == NULL) {
            break; // Stop if no more tokens are found
        }
    }

    return result;
}


void get_server_info(char* response){
    char** info = separator(response, ';', INFO);
    printf("Server info:\n");
    printf("Hostname: %s\n", info[0]);
    printf("Base path directory: %s\n", info[1]);
    printf("IP Address: %s\n", info[2]);
    for (int i = 0; i < 3; i++){
        free(info[i]);
    }
    free(info);
}

void get_metrics(char* response){
    char** metrics = separator(response, ';', METR);
    printf("Server metrics:\n");
    printf("Total messages: %s\n", metrics[0]);
    printf("Total bytes: %s\n", metrics[1]);
    printf("Total historic connections: %s\n", metrics[2]);
    printf("Max consecutive connections: %s\n", metrics[3]);
    for (int i = 0; i < 4; i++){
        free(metrics[i]);
    }
    free(metrics);

}

int recv_response(char* command){

    int state = get_command(command);
    if (state == -1){
        return 1;
    }
    memset(response_buffer, 0, BUFFER_SIZE);

    if (  state == LOGG ){
        while(1){
            int bytes_received = recv(pop3_socket, response_buffer, BUFFER_SIZE - 1, 0);
            if (bytes_received < 0) {
                perror("Failed to receive response");
                return 1; // Indica error en la recepción
            }
            if (bytes_received == 0) {
                break;
            }
            response_buffer[bytes_received] = '\0';
            printf("%s", response_buffer);
            if (strstr(response_buffer, ".\r\n") != NULL) {
                break; // Encuentra el final del mensaje y sale del bucle
        }
        
        }
    }else {

        int bytes_received = recv(pop3_socket, response_buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            perror("Failed to receive response");
            return 1; // Indica error en la recepción
        }
        response_buffer[bytes_received] = '\0';
            
    printf("Server response: %s\n", response_buffer);
        if ( strncmp(response_buffer, "+OK", 3) == 0 ){
            switch(state){
                case CONFIG:
                    printf("Configuration updated successfully.\n");
                    break;
                case INFO:
                    get_server_info(response_buffer+4);
                    break;
                case METR:
                    get_metrics(response_buffer+4);
                    break;
            }
        }
        else{
            return 1;
        }
    }
    return 0;

}

void verify_server_status(void) {
    clock_t start_time = clock();

    // Envío un NOOP para chequear el estado del servidor
    if (send_command("NOOP" LINE_END) == 1) {
        connection_status = OFFLINE;
    } else {
        char temp[BUFFER_SIZE];
        recv(pop3_socket, temp, BUFFER_SIZE - 1, 0);
        connection_status = ONLINE;
    }

    clock_t end_time = clock();

    response_time_ms = ((double)(end_time - start_time) * 1000) / CLOCKS_PER_SEC;
}



int get_connection_status(void) {
    return connection_status == ONLINE ? ONLINE : OFFLINE;
}

double get_response_time(void) {
    return response_time_ms;
}


int get_command(char* command){

    if ( strncmp(command, "PORT", 4) == 0 || strncmp(command, "HOST", 4) == 0  || strncmp(command, "DIRR", 4) == 0 || strncmp(command, "TRAN", 4) == 0 || strncmp(command, "IPV6", 4) == 0){
        return CONFIG;
    }
    else if ( strncmp(command, "INFO", 4) == 0 ){
        return INFO;
    }
    else if ( strncmp(command, "METR", 4) == 0 ){
        return METR;
    }
    else if ( strncmp(command, "LOGG", 4) == 0 ){
        return LOGG;
    }
    else{
        return -1;
    }
}

