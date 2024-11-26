#include <stdio.h>     /* for printf */
#include <stdlib.h>    /* for exit */
#include <limits.h>    /* LONG_MIN et al */
#include <string.h>    /* memset */
#include <errno.h>
#include <getopt.h>

#include "args.h"

char** strsep( char* str, char delim );

static unsigned short
port(const char *s) {
     char *end     = 0;
     const long sl = strtol(s, &end, 10);

     if (end == s|| '\0' != *end
        || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)
        || sl < 0 || sl > USHRT_MAX) {
         fprintf(stderr, "port should in in the range of 1-65536: %s\n", s);
         exit(1);
         return 1;
     }
     return (unsigned short)sl;
}


char** strsep( char* str, char delim ) {
    int i = 0;
    char** result = calloc(2, sizeof(char*));
    result[0] = calloc(1,sizeof(char)*USER_SIZE);
    result[1] = calloc(1,sizeof(char)*USER_SIZE);
    
    while ( str[i] != '\0' && str[i] != delim ) {
        result[0][i] = str[i];
        i++;
    }
    if ( str[i] == '\0' ) {
        free(result[0]);
        free(result[1]);
        return NULL;
    }
    result[0][i] = '\0';
    int j = 0;
    i++;

    while ( str[i] != '\0' ) {
        result[1][j] = str[i];
        i++;
        j++;
    }
    result[1][j] = '\0';

    return result;
}

void split_t_arg( char* original, char** path, char** right_hand_arg){
    int i = 0;
    printf("original: %s\n", original);
    while (original[i] != ':' && original[i] != '\0') {
        i++;
    }

    *path = calloc(i + 1, sizeof(char));
    snprintf(*path, i + 1, "%s", original);

    int len = strlen(original + i + 1);
    *right_hand_arg = calloc(len + 1, sizeof(char));

    if (original[i] == ':') {
        snprintf(*right_hand_arg, len + 1, "%s", original + i + 1);
    } else {
        free(*right_hand_arg);
        *right_hand_arg= NULL;
    }
}

static char* users[] = {
    "user1:pass1",
    "user2:pass2",
    "user3:pass3",
    "user4:pass4",
    "user5:pass5",
    "user6:pass6"
};


static void
user(char *s, user_list_header* users) {

    user_list* user = calloc(1, sizeof(user_list));
    if (users->list == NULL) {
        users->list = user;
    }else{
        user_list* aux = users->list;
        while ( aux->next != NULL ){
            aux = aux->next;
        }
        aux->next = user;
    }

    char** p = strsep(s, ':');
    if(p == NULL) {
        fprintf(stderr, "password not found\n");
        exit(1);
    } else {
        user->name = p[0];
        user->pass = p[1];
    }

    users->size++;
    free(p);
}

static void
version(void) {
    fprintf(stderr, "POP3 version 0.5\n"
                    "ITBA Protocolos de Comunicación 2024/2 -- Grupo G\n"
                    "AQUI VA LA LICENCIA\n");
}

/*
Estructura de la carpeta para el argumento -d:
$ tree .
.
└── user1
    ├── cur
    ├── new
    │   └── mail1
    └── tmp
*/
static void
usage(const char *progname) {
    fprintf(stderr,
            "Usage: %s [OPTION]...\n"
            "\n"
            "   -h                   Imprime la ayuda y termina.\n"
            "   -l <POP3 addr>       Dirección donde servirá el servidor POP.\n"
            "   --ip <ip>            Dirección IP donde servirá el servidor POP.\n"
            "   -L <conf  addr>      Dirección donde servirá el servicio de management.\n"
            "   -p <POP3 port>       Puerto entrante conexiones POP3.\n"
            "   --port <port>        Puerto entrante conexiones POP3.\n"
            "   -P <conf port>       Puerto entrante conexiones configuracion\n"
            "   -u <name>:<pass>     Usuario y contraseña de usuario que puede usar el servidor. Hasta 10.\n"
            "   -v                   Imprime información sobre la versión versión y termina.\n"
            "   -d                   Carpeta donde residen los Maildirs\n"
            "   --path <path>        Carpeta donde residen los Maildirs\n"
            "   -N                   Más estadisticas sobre el server\n"
            "   --default            Utiliza los defaults de usuarios y configuración. \n"
            "--host <host>           Hostname del servidor.\n"
            "-T <path to the binary> Camino hasta el binario que va a realizar la transformacion.\n"
            "\n",
            progname);
    exit(1);
}

void 
parse_args(const int argc, char **argv, pop3_structure *args) {

    // defaults
    args->user_list = calloc(1, sizeof(user_list_header));
    args->base_dir = "./src/root/";
    args->host = "localhost";
    args->ip = "::1";
    args->port = DEFAULT_PORT;
    args->trans_enabled = false;
    // para analisis de trafico
    args->disectors_enabled = false;
    args->mng_port = DEFAULT_MGMT_PORT;
    args->mng_ip = "::1";
    args->mng_socket = 0;

    int c;
    int nusers = 0;

    while (true) {
        int option_index = 0;
        static struct option long_options[] = {
            { "ip",    required_argument, 0, 0xD001 },
            { "port",  required_argument, 0, 0xD002 },
            { "host",  required_argument, 0, 0xD003 },
            { "path",  required_argument, 0, 0xD004 },
            { "default",  no_argument, 0, 0xD005 },
            { 0,           0,                 0, 0 }
        };

        c = getopt_long(argc, argv, "hl:L:N:P:u:v:T:d:p:N", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
            case 'h':
                usage(argv[0]);
                break;
            case 'l':
                args->ip = optarg;
                break;
            case 'L':
                args->mng_ip = optarg;
                break;
            case 'N':
                args->disectors_enabled = true;
                break;
            case 'd':
                args->base_dir = optarg;
                break;
            case 'p':
                args->port = port(optarg);
                break;
            case 'P':
                args->mng_port = port(optarg);
                break;
            case 'T':
                if(optarg == NULL){
                    fprintf(stderr, "no binary path provided.\n");
                    exit(1);
                }

                args->trans = calloc(1, sizeof(transformation_structure));
                args->trans_enabled = true;

                split_t_arg(optarg, &args->trans->trans_binary_path, &args->trans->trans_args);
                break;
            case 'u':
                if(nusers >= MAX_USERS) {
                    fprintf(stderr, "maximun number of command line users reached: %d.\n", MAX_USERS);
                    exit(1);
                } else {
                    user(optarg, args->user_list);
                    nusers++;
                }
                break;
            case 'v':
                version();
                exit(0);
                break;
            case 0xD005:
                for (int i = 0; i < 6; i++) {
                    user(users[i], args->user_list);
                }
                // asigno devuelta por si los cambiaron
                args->base_dir = "./src/root/";
                args->host = "localhost";
                args->ip = "::1";
                args->trans_enabled = false;
                args->port = DEFAULT_PORT;
                break;
            case 0xD001:
                args->ip = optarg;
                break;
            case 0xD002:
                args->port = port(optarg);
                break;
            case 0xD003:
                args->host = optarg;
                break;
            case 0xD004:
                args->base_dir = optarg;
                break;
            default:
                fprintf(stderr, "unknown argument %d.\n", c);
                exit(1);
        }

    }
    if (optind < argc) {
        fprintf(stderr, "argument not accepted: ");
        while (optind < argc) {
            fprintf(stderr, "%s ", argv[optind++]);
        }
        fprintf(stderr, "\n");
        exit(1);
    }
}
