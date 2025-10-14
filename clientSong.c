#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define FIFO_CLIENT_TO_SERVER "/tmp/client_to_server"
#define FIFO_SERVER_TO_CLIENT "/tmp/server_to_client"
#define MAX_BUF 1024

int main() {
    char artist[512], song[512], buffer[MAX_BUF];

    printf("Nombre del artista: ");
    fgets(artist, sizeof(artist), stdin);
    artist[strcspn(artist, "\n")] = 0;

    printf("Nombre de la canción: ");
    fgets(song, sizeof(song), stdin);
    song[strcspn(song, "\n")] = 0;

    int fd_out = open(FIFO_CLIENT_TO_SERVER, O_WRONLY);
    sprintf(buffer, "%s|%s", artist, song);
    write(fd_out, buffer, strlen(buffer) + 1);
    close(fd_out);

    int fd_in = open(FIFO_SERVER_TO_CLIENT, O_RDONLY);
    read(fd_in, buffer, sizeof(buffer));
    close(fd_in);

    printf("\n📄 Resultado recibido:\n%s\n", buffer);
    return 0;
}
