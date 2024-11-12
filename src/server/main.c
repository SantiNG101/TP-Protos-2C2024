
#include "../shared/args.h"

static bool done = false;


static void
sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}



int main( const int argc, char **argv ) {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);

    pop3_structure* pop3_struct = calloc(1, sizeof(pop3_structure));
    pop3_struct->port = DEFAULT_PORT;

    // agruments

    parse_args(argc, argv, pop3_struct);


    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(pop3_struct->port);

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
    printf("POP3 Server started on port %d...\n", pop3_struct->port);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_size);
        
        if (client_socket < 0) {
            perror("Client connection failed");
            break;
        }
        printf("Client connected.\n");
        pop3_struct->cli_socket = client_socket;
        handle_client(pop3_struct);
    }
    free_pop3_structure(pop3_struct);
    free(pop3_struct);
    close(server_socket);

    return 0;
}


