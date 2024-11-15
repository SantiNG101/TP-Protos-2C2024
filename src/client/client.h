#ifndef CLIENT_H
#define CLIENTE_H

#include "../shared/parser.h"
#include "../server/pop3.h"

void on_client_connect(Client_data *client);

void on_client_disconnect(Client_data *client);
#endif

