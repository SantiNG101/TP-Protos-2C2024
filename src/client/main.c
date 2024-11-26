#include <stdio.h>
#include <stdlib.h>
#include "./header/monitoring_handler.h"

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

    if (retrieve_pop3_stats() != 0) {
        fprintf(stderr, "Error retrieving statistics from the server.\n");
        disconnect_from_server();
        return EXIT_FAILURE;
    }


    printf("Verifying server status...\n");
    verify_server_status();

    printf("Server Status: %s\n", get_connection_status());
    printf("Response Time: %.2f ms\n", get_response_time());
    printf("Total Messages: %d\n", get_total_messages());
    printf("Total Bytes: %d\n", get_total_bytes());

    printf("Disconnecting from server...\n");
    disconnect_from_server();
    printf("Disconnected successfully.\n");

    return EXIT_SUCCESS;
}