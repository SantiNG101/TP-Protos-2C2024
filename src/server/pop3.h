#ifndef POP3_H
#define POP3_H

#include <stdio.h>
#include <stdlib.h>  // malloc
#include <string.h>  // memset
#include <sys/stat.h>
#include <assert.h>  // assert
#include <errno.h>
#include <time.h>
#include <unistd.h>  // close
#include <pthread.h>
#include <arpa/inet.h>
#include <limits.h>
#include <signal.h>
#include <sys/types.h>   // socket
#include <sys/socket.h>  // socket
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <dirent.h>
#include <sys/poll.h>
#include <fcntl.h>

#include "../shared/buffer.h"

#define DEFAULT_PORT 1110
#define DEFAULT_MGMT_PORT 1111
#define BUFFER_SIZE 1024
#define USER_SIZE 256

enum pop3_state {
    // se fija que le pidieron
    READ,
    // checking new mails
    CHECKING_NEW,
    // checking old mails
    CHECKING_OLD,
    // deleting mails
    DELETING,
    // escribe el mail pedido anteriormente
    WRITE,
    // obtiene el mail para un usuario
    PROCESSING,
    // estados terminales
    DONE,
    ERROR_STATE
};

enum pop3_command {
    USER,
    PASS,
    STAT,
    LIST,
    RETR,
    DELE,
    NOOP,
    RSET,
    QUIT,
    ERROR_COMMAND = -1
};

typedef struct user_list {
    char* name;
    char* pass;
    struct user_list* next;
} user_list;

typedef struct user_list_header {
    user_list* list;
    int size;
} user_list_header;

typedef struct file_list {
    char* name;
    int size;
    bool deleted;
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

typedef struct pop3_structure {
    user_list_header* user_list;
    maildir* maildir;
    char* base_dir;
    char* host;
    char* ip;
    int port;
    int server_socket;
    int cli_socket;
    // para analisis de trafico
    bool disectors_enabled;
    int mng_port;
    char* mng_ip;
    int mng_socket;
}pop3_structure;

enum client_state {
    AUTHORIZATION,
    TRANSACTION,
    CLOSING,
    ERROR_CLIENT = -1
};

typedef struct Client_data {
    struct pop3_structure* pop3;
    user_list* user;

    char send_buffer[BUFFER_SIZE];
    char recv_buffer[BUFFER_SIZE];
    
    enum client_state client_state;
} Client_data;

void handle_client(Client_data* client_data);
void free_pop3_structure( pop3_structure* pop3_struct );


#endif