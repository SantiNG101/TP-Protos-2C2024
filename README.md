# TP-Protos-2C2024

Compilacion: 

Ejecutar el comando make clean all desde la carpeta TP-PROTOS-2C2024
Los binarios se generaran en la carpeta ./bin

Ejecucion:

Los siguientes son los argumentos posibles para la ejecucion del servidor

./bin/server

o se puede correr con argumentos
(hacer ./bin/server -h para obtener mas informacion sobre los argumentos posibles)

Por ejemplo:

./bin/server -T /usr/bin/sed:'s/e/x/g'

En caso de que sean transformaciones, aceptamos un argumento que tiene que estar separado por un ':'
No se aceptan flags.

Para el client:

(Si se utilizo la opcion -P, se debe usar ese puerto)
Por default es el 1111

./bin/client [direccion IPV6 o IPV4] [puerto de management]
