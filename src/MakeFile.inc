
COMPILER = ${CC}

CFLAGS = --std=c11 -Wall -pedantic -pedantic-errors -Wformat=2 -Wextra -Wno-unused-parameter -Wundef -Wuninitialized -Wno-implicit-fallthrough -D_POSTFIX_C_SOURCE=200809L -fsanitize=address -g -O3 

