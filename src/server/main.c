
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
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);

    // agruments

    user_list_header* user_list = calloc(1, sizeof(user_list_header));

    user_list->list = make_user_list(users, 6);
    user_list->size = 6;

    int actual_port = PORT;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(actual_port);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Binding failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 500) < 0) {
        perror("Listening failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    printf("POP3 Server started on port %d...\n", actual_port);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_size);
        
        if (client_socket < 0) {
            perror("Client connection failed");
            break;
        }
        printf("Client connected.\n");

        handle_client(client_socket, user_list);
    }

    close(server_socket);
    return 0;
}


