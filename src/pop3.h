#ifndef POP3_H
#define POP3_H

#include<stdio.h>
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

#include "selector.h"
#include "request.h"
#include "buffer.h"
//#include "stm.h"
#include"netutils.h"

#define PORT 1110
#define BUFFER_SIZE 1024
#define BASE_DIR "./root/"

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
    QUIT,
    ERROR_COMMAND = -1
};
/*
struct pop3 {
    struct state_machine stm;
}
*/

void handle_client(int client_socket);
int client_validation(buffer* buff);
int password_validation(buffer* buff);


#endif