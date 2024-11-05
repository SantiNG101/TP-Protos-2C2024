#include "client.h"

#include <stdlib.h>

Client* initializing_client(){
    Client* client = malloc(sizeof(*client));
    client->state = VALIDATION;
    client->user = NULL;
    client->total_mail = 0;
    client->not_deleted_mail = 0;

    return client;
}

void free_client(Client* client){
    free(client);
}
