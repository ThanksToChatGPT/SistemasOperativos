#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define FIFO_CLIENT_TO_SERVER "/tmp/client_to_server"
#define FIFO_SERVER_TO_CLIENT "/tmp/server_to_client"
#define MAX_BUF 1026

int main() {
    char artist[512], song[512], buffer[MAX_BUF];

    printf("Nombre del artista: ");
    if(!fgets(artist, sizeof(artist), stdin)){
        printf("Error al leer el nombre del artista.\n");
        return -1;
    }
    artist[strcspn(artist, "\n")] = 0;

    printf("Nombre de la canción: ");
    if(!fgets(song, sizeof(song), stdin)){
        printf("Error al leer el nombre de la canción.\n");
        return -1;
    }
    song[strcspn(song, "\n")] = 0;

    if (artist[0] == '\0' || song[0] == '\0') {
        printf("Entradas vacías no permitidas\n");
    return 1;
}

    int fd_out = open(FIFO_CLIENT_TO_SERVER, O_WRONLY);
    if (fd_out == -1) {
    printf("Error al abrir el FIFO para escribir.\n");
    return 1;
    }

    sprintf(buffer, "%s|%s", artist, song);
    write(fd_out, buffer, strlen(buffer) + 1);
    close(fd_out);

    int fd_in = open(FIFO_SERVER_TO_CLIENT, O_RDONLY);
    if (fd_in == -1) {
        printf("Error al abrir el FIFO para leer.\n");
        return -1;
    }

    read(fd_in, buffer, sizeof(buffer));
    
    if(close(fd_in) == -1){
        printf("Error al cerrar el FIFO de lectura.\n");
        return -1;
    }

    printf("\nResultado recibido:\n%s\n", buffer);
    return 0;
}
