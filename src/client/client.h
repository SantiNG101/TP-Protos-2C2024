#ifndef CLIENT_H
#define CLIENTE_H

#include "parser.h"

typedef enum {
    LOGGED = 1,
    NOT_LOGGED = 2,
    TIMEOUT = 3
} status;

struct client {

    // autentication
    char *user;
    char *pass;

}

#endif

