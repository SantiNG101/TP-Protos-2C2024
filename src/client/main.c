#include "./header/monitoring_handler.h"
#include "../shared/args.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_address> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *server_address = argv[1];
    char *port_number = argv[2];

    printf("Connecting to POP3 server at %s:%s...\n", server_address, port_number);
    if (connect_to_pop3_server(server_address, port_number) != 0) {
        fprintf(stderr, "Failed to connect or authenticate with the POP3 server.\n");
        return EXIT_FAILURE;
    }
    printf("Connected and authenticated successfully.\n");



    printf("Verifying server status...\n");
    verify_server_status();

    while (get_connection_status() == ONLINE) {

        char CMD[BUFFER_SIZE];
        int state;

        printf("Server Status: %d\n", get_connection_status());
        printf("Response Time: %.2f ms\n", get_response_time());

        printf("Enter a command (STAT, INFO, QUIT): ");

        fgets(CMD, BUFFER_SIZE, stdin);
        // Remove newline character if it exists
        size_t len = strlen(CMD);
        if (len > 0 && CMD[len - 1] == '\n') {
            CMD[len - 1] = '\0'; // Remove the newline character
        }

        // Append '\r' and '\n' to the string
        CMD[len-1] = '\r';
        CMD[len ] = '\n';
        CMD[len +1] = '\0';

        if ( strncmp(CMD, "QUIT", 4) == 0 ){
            printf("Disconnecting from server...\n");
            disconnect_from_server();
            break;
        }

        state = send_command(CMD);
        if (state == 1){
            printf("Error sending command.\n");
            verify_server_status();
            continue;
        }

        state = recv_response(CMD);
        if (state == 1){
            printf("Error receiving response.\n");
        }
        verify_server_status();
    }

    // a modificar: port / host / base_dir / trans_enabled
        /*
        PORT <port number>
        HOST <name>
        DIRR <path>
        TRAN <1/0>
        IPV6 <new_ip>
        METR
        INFO => de base_dir, host, ip actual
        LOGG
    */

    printf("Disconnected successfully.\n");

    return EXIT_SUCCESS;
}