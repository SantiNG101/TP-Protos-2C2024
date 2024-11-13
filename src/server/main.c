#include "pop3.h"


// A 1024 sockets. 1 server 1 cliente -> 512 clientes max en simultaneo
// => 512 eventos maximo en simultaneo 

#define MAX_CLIENTS 512
#define MAX_EVENTS 512

static int client_count = 0;

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

struct Client_data clients[MAX_CLIENTS];

struct pollfd pollfds[MAX_CLIENTS + 1];

void init_pollfds() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        pollfds[i].fd = -1;
        pollfds[i].events = POLLIN;
    }
}

void init_clientData(){
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].client_state = AUTHORIZATION;
        clients[i].pop3 = calloc(1, sizeof(pop3_structure));
        clients[i].user = calloc(1, sizeof(User));
    }
}

int add_client(int client_fd) {
    for (int i = 1; i <= MAX_CLIENTS; i++) {
        if (pollfds[i].fd == -1) {
            pollfds[i].fd = client_fd;
            pollfds[i].events = POLLIN;
            return 0;
        }
    }
    return -1;
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
    struct sockaddr_in6 server_addr;

    server_socket = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 0;
    if (setsockopt(server_socket, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) < 0) {
        perror("Failed to set IPV6_V6ONLY");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_addr = in6addr_any; 
    server_addr.sin6_port = htons(PORT);

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
    printf("Server started on port %d, accepting IPv4 and IPv6 connections...\n", PORT);

    init_pollfds();
    init_clientData();

    pollfds[0].fd = server_socket;
    pollfds[0].events = POLLIN;

    while (1) {
        int poll_count = poll(pollfds, client_count + 1, -1);

        if (poll_count < 0) {
            perror("poll failed");
            break;
        }

        for (int i = 0; i <= client_count; i++) {
            if (pollfds[i].revents & POLLIN) {
                pollfds[i].revents = 0;

                if (pollfds[i].fd == server_socket) {
                    struct sockaddr_storage client_addr;
                    socklen_t addr_len = sizeof(client_addr);
                    int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len);
                    if (client_socket < 0) {
                        perror("Accept failed");
                    } else {
                        if (add_client(client_socket) < 0) {
                            printf("Too many clients\n");
                            close(client_socket);
                        } else {
                            client_count++;
                            printf("Client connected.\n");
                        }
                    }
                } else {
                    handle_client(pollfds[i].fd, NULL, &clients[i]);
                }
            }
        }
    }

    close(server_socket);
    return 0;
}
