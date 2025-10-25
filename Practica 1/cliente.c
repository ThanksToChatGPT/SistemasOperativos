#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main() {
    int fd, r;
    struct sockaddr_in cliente;
    #define PORT 3535
    // Para TCP se debe usar (SOCK_STREAM).
    // Crear socket UDP
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("error al crear socket");
        exit(-1);
    }

    // Configurar dirección del servidor al que se conecta
    cliente.sin_family = AF_INET;
    cliente.sin_port = htons(PORT);
    cliente.sin_addr.s_addr = inet_addr("127.0.0.1");
    bzero(&cliente.sin_zero, 8);

    // Conectarse al servidor
    r = connect(fd, (struct sockaddr*)&cliente, sizeof(struct sockaddr_in));
    if (r == -1) {
        perror("error en connect");
        close(fd);
        exit(-1);
    }

    // Recibir mensaje del servidor
    char buffer[20];
    r = recv(fd, buffer, 20, 0);
    if (r == -1) {
        perror("error en recv");
        close(fd);
        exit(-1);
    }
    buffer[r] = '\0';  // asegurar terminación de string
    printf("%s\n", buffer);

    // Enviar mensaje al servidor
    r = send(fd, "hola servidor", 13, 0);
    if (r == -1) {
        perror("error en send");
        close(fd);
        exit(-1);
    }

    // Cerrar socket
    close(fd);

    return 0;
}
