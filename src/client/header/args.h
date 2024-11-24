#ifndef CLIENT_ARGS_H
#define CLIENT_ARGS_H

#include "../shared/buffer.h"
#include "../server/pop3.h"

static void usage(const char *progname);

void parse_args(const int argc, char **argv, pop3_structure *args);

#endif