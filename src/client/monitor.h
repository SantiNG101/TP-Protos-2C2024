#ifndef MONITOR_H
#define MONITOR_H
#include <stdint.h>
#include <stddef.h>
#include <time.h>

typedef struct {
    uint64_t total_connections;
    uint64_t current_connections;
    uint64_t total_bytes_sent;
    uint64_t total_commands_processed;
    uint64_t total_errors;
    time_t server_start_time;
} Monitor;

extern Monitor *global_monitor;

void increment_connection_count(void);
void decrement_connection_count(void);
void increment_bytes_sent(size_t bytes);
void increment_command_count(void);
void increment_error_count(void);
void reset_metrics(void);

#endif // MONITOR_H