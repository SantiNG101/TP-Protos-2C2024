
#include "pop3.h"

/*
Estructura de la carpeta para el argumento -d:
$ tree .
.
└── user1
    ├── cur
    ├── new
    │   └── mail1
    └── tmp
*/
static void
usage(const char *progname) {
    fprintf(stderr,
            "Usage: %s [OPTION]...\n"
            "\n"
            "   -h               Imprime la ayuda y termina.\n"
            "   -l <POP3 addr>   Dirección donde servirá el servidor POP.\n"
            "   -L <conf  addr>  Dirección donde servirá el servicio de management.\n"
            "   -p <POP3 port>   Puerto entrante conexiones POP3.\n"
            "   -P <conf port>   Puerto entrante conexiones configuracion\n"
            "   -u <name>:<pass> Usuario y contraseña de usuario que puede usar el servidor. Hasta 10.\n"
            "   -v               Imprime información sobre la versión versión y termina.\n"
            "   -d               Carpeta donde residen los Maildirs"
            "\n",
            progname);
    exit(1);
}

static bool done = false;
char** strsep( char* str, char delim );

static void
sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}

user_list* make_user_list( char* users[], int amount ) {
    
    if ( amount == 0 ) {
        return NULL;
    }
    user_list* start = calloc(1, sizeof(user_list));
    user_list* current = start;
    int i = 0;

    while(amount--) {
        char** user = strsep(users[i], ':');
        current->name = user[0];
        current->pass = user[1];
        if ( amount > 0 ) {
            current->next = calloc(1, sizeof(user_list));
            current = current->next;
        }else{
            current->next = NULL;
        }
    }

    return start;
}

void send_file(int client_socket, const char *filename) {
    int file = open(filename, O_RDONLY);
    if (file < 0) {
        perror("Failed to open file");
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read, bytes_sent;

    while ((bytes_read = read(file, buffer, sizeof(buffer))) > 0) {
        bytes_sent = send(client_socket, buffer, bytes_read, 0);
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
    shutdown(client_socket, SHUT_WR);
}

char** strsep( char* str, char delim ) {
    int i = 0;
    char** result = calloc(2, sizeof(char*));
    result[0] = malloc(sizeof(char)*USER_SIZE);
    result[1] = malloc(sizeof(char)*USER_SIZE);
    
    while ( str[i] != '\0' && str[i] != delim ) {
        result[0][i] = str[i];
        i++;
    }
    result[0][i] = '\0';
    int j = 0;
    i++;

    while ( str[i] != '\0' ) {
        result[1][j] = str[i];
        i++;
        j++;
    }
    result[1][j] = '\0';

    return result;
}

static char* users[] = {
    "user1:pass1",
    "user2:pass2",
    "user3:pass3",
    "user4:pass4",
    "user5:pass5",
    "user6:pass6"
};

int main() {
    int server_socket;
    struct sockaddr_in server_addr;
    
    server_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket to address and port
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Binding failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }


    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listening failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    printf("Server started on port %d...\n", PORT);

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1 failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    struct epoll_event event;
    event.events = EPOLLIN;  
    event.data.fd = server_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event) == -1) {
        perror("epoll_ctl failed");
        close(server_socket);
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    struct epoll_event events[MAX_EVENTS];

    while (1) {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_events < 0) {
            perror("epoll_wait failed");
            break;
        }

        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == server_socket) {
                struct sockaddr_in client_addr;
                socklen_t addr_size = sizeof(client_addr);
                int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_size);
                if (client_socket == -1) {
                    perror("Client connection failed");
                    continue;
                }

                int flags = fcntl(client_socket, F_GETFL, 0);
                fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);

                struct epoll_event client_event;
                client_event.events = EPOLLIN | EPOLLET; 
                client_event.data.fd = client_socket;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &client_event) == -1) {
                    perror("epoll_ctl failed for client socket");
                    close(client_socket);
                    continue;
                }
                printf("Accepted new client connection\n");

            } else {
                handle_client(events[i].data.fd, NULL);
            }
        }
    }

    close(server_socket);
    close(epoll_fd);
    return 0;
}

