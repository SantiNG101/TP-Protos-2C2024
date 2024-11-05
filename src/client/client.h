#ifndef CLIENT_H
#define CLIENTE_H

#include "../ignore/parser.h"

typedef enum {
    VALIDATION = 1,
    LOGGED = 2,
    TIMEOUT = 3
} status;

struct users {
    char *user;
    char *pass;
};

typedef struct Client{
    status state;
    struct users* user;
    int total_mail;
    int not_deleted_mail;
} Client;

Client* initializing_client();
void free_client(Client* client);

#endif

