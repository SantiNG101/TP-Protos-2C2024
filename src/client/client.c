#include <stdlib.h>
#include "client.h"
#include <stdlib.h>
#include "monitor.h"
#include "../server/pop3.h"

// Al establecer una nueva conexión
void on_client_connect(Client_data *client) {
    increment_connection_count();
    // El resto de la lógica?...
}
// Al cerrar una conexión
void on_client_disconnect(Client_data *client) {
    decrement_connection_count();
    // El resto de la lógica?...
}