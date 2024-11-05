
include ./Makefile.inc

#all: server client
# Archivos fuente y objetos
SERVER_SOURCES=$(wildcard src/server/*.c)
CLIENT_SOURCES=$(wildcard src/client/*.c)
SHARED_SOURCES=$(wildcard src/shared/*.c)

SERVER_OBJECTS=$(SERVER_SOURCES: src/%.c=obj/%.o)
CLIENT_OBJECTS=$(CLIENT_SOURCES: src/%.c=obj/%.o)
SHARED_OBJECTS=$(SHARED_SOURCES: src/%.c=obj/%.o)

OUTPUT_FOLDER=./bin
OBJECTS_FOLDER=./obj
SERVER_OUTPUT_FILE=$(OUTPUT_FOLDER)/server
CLIENT_OUTPUT_FILE=$(OUTPUT_FOLDER)/client

# Regla principal para compilar todo
all: server client

# Regla para compilar el ejecutable a partir de los objetos
server: $(SERVER_OUTPUT_FILE)

client: $(CLIENT_OUTPUT_FILE)

$(SERVER_OUTPUT_FILE): $(SERVER_OBJECTS) $(SHARED_OBJECTS)
	mkdir -p $(OUTPUT_FOLDER)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SERVER_OBJECTS) $(SHARED_OBJECTS) -o $(SERVER_OUTPUT_FILE)

$(CLIENT_OUTPUT_FILE): $(CLIENT_OBJECTS) $(SHARED_OBJECTS)
	mkdir -p $(OUTPUT_FOLDER)
	$(CC) $(CFLAGS) $(LDFLAGS) $(CLIENT_OBJECTS) $(SHARED_OBJECTS) -o $(CLIENT_OUTPUT_FILE)

obj/%.o: src/%.c
	mkdir -p $(OBJECTS_FOLDER)
	mkdir -p $(OBJECTS_FOLDER)/client
	mkdir -p $(OBJECTS_FOLDER)/server
	mkdir -p $(OBJECTS_FOLDER)/shared
	$(CC) $(CFLAGS) -c $< -o $@

# Regla para limpiar archivos compilados
clean:
	rm -rf $(OUTPUT_FILE)
	rm -rf $(OBJECTS_FOLDER)

.PHONY: all clean server client