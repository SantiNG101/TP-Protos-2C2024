#include <stdio.h>
#include <stlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include "../client/header/args.h"

static void
usage(const char *progname) {
    fprintf(stderr,
            "Usage: %s [OPTION]...\n"
            "\n"
            "   -h               Imprime la ayuda y termina.\n"
            "   -l <POP3 addr>   Dirección donde servirá el servidor POP.\n"
            "   --ip <ip>        Dirección IP donde servirá el servidor POP.\n"
            "   -p <POP3 port>   Puerto entrante conexiones POP3.\n"
            "   --port <port>    Puerto entrante conexiones POP3.\n"
            "   -P <conf port>   Puerto entrante conexiones configuracion\n"
            "   -v               Imprime información sobre la versión versión y termina.\n"
            "   -N               Más estadisticas sobre el server\n"
            "\n",
            progname);
    exit(1);
}

void 
parse_args(const int argc, char **argv, pop3_structure *args) {

    // defaults
    args->maildir = NULL;
    args->user_list = calloc(1, sizeof(user_list_header));
    args->base_dir = "./src/root/";
    args->host = "localhost";
    args->ip = "::1";
    args->port = DEFAULT_PORT;
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
            { 0,           0,                 0, 0 }
        };

        c = getopt_long(argc, argv, "hl:Np:P:v", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
            case 'h':
                usage(argv[0]);
                break;
            case 'l':
                args->ip = optarg;
                break;
            case 'N':
                args->disectors_enabled = true;
                break;
            case 'p':
                args->port = port(optarg);
                break;
            case 'P':
                args->mng_port = port(optarg);
                break;
            case 'v':
                version();
                exit(0);
                break;
            case 0xD001:
                args->ip = optarg;
                break;
            case 0xD002:
                args->port = port(optarg);
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
