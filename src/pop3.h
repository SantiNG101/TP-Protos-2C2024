#ifndef POP3_H
#define POP3_H

#include<stdio.h>
#include <stdlib.h>  // malloc
#include <string.h>  // memset
#include <assert.h>  // assert
#include <errno.h>
#include <time.h>
#include <unistd.h>  // close
#include <pthread.h>
#include <arpa/inet.h>

#include "request.h"
#include "buffer.h"
#include "stm.h"
#include"netutils.h"


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
    ERROR
}

struct pop3 {

    struct state_machine stm;
    



}


#endif