#include "monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

Monitor *global_monitor = NULL;

void init_monitor(void) {
    global_monitor = (Monitor *)calloc(1, sizeof(Monitor));
    if (global_monitor != NULL) {
        global_monitor->server_start_time = time(NULL);
    }
}

void free_monitor(void) {
    if (global_monitor != NULL) {
        free(global_monitor);
        global_monitor = NULL;
    }
}

void increment_connection_count(void) {
    if (global_monitor != NULL) {
        global_monitor->total_connections++;
        global_monitor->current_connections++;
    }
}

void decrement_connection_count(void) {
    if (global_monitor != NULL && global_monitor->current_connections > 0) {
        global_monitor->current_connections--;
    }
}

void increment_bytes_sent(size_t bytes) {
    if (global_monitor != NULL) {
        global_monitor->total_bytes_sent += bytes;
    }
}

void increment_command_count(void) {
    if (global_monitor != NULL) {
        global_monitor->total_commands_processed++;
    }
}

void increment_error_count(void) {
    if (global_monitor != NULL) {
        global_monitor->total_errors++;
    }
}

void reset_monitor(void) {
    if (global_monitor != NULL) {
        global_monitor->total_connections = 0;
        global_monitor->current_connections = 0;
        global_monitor->total_bytes_sent = 0;
        global_monitor->total_commands_processed = 0;
        global_monitor->total_errors = 0;
        global_monitor->server_start_time = time(NULL);
    }
}
