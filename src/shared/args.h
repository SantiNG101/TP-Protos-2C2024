#ifndef ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8
#define ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8

#include <stdbool.h>
#include "../server/pop3.h"

#define MAX_USERS 10

struct doh {
    char           *host;
    char           *ip;
    unsigned short  port;
    char           *path;
    char           *query;
};


/**
 * Interpreta la linea de comandos (argc, argv) llenando
 * args con defaults o la seleccion humana. Puede cortar
 * la ejecución.
 */
void 
parse_args(const int argc, char **argv, pop3_structure *args);
char** strsep( char* str, char delim );

#endif

