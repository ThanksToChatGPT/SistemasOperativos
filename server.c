#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>

#define BACKLOG 5

int conectar(int port) {
    int fd, r;
    struct sockaddr_in server;

    // Crear socket TCP
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("Error al crear socket");
        return -1;
    }

    // Configurar estructura servidor
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;
    bzero(&server.sin_zero, 8);

    // Hacer bind
    r = bind(fd, (struct sockaddr*)&server, sizeof(struct sockaddr));
    if (r == -1) {
        perror("Error en bind");
        close(fd);
        return -1;
    }

    // Escuchar conexiones entrantes
    r = listen(fd, BACKLOG);
    if (r == -1) {
        perror("Error en listen");
        close(fd);
        return -1;
    }

    return fd;
}

void* manejar_cliente(void *arg) {
    int fd_cliente = (intptr_t)arg;
    int r;

    // Enviar mensaje al cliente
    r = send(fd_cliente, "Servidor conectado", 18, 0);
    if (r == -1) {
        perror("Error en send");
        close(fd_cliente);
        pthread_exit(NULL);
    }

    // Recibir mensaje del cliente
    char buffer[256];
    r = recv(fd_cliente, buffer, sizeof(buffer), 0);
    if (r == -1) {
        perror("Error en recv");
        close(fd_cliente);
        pthread_exit(NULL);
    }
    buffer[r] = '\0';
    printf("Cliente: %s\n", buffer);

    // Cerrar socket del cliente
    close(fd_cliente);
    pthread_exit(NULL);
}



int main(int argc, char *argv[]) {
    int port = atoi(argv[1]);
    
    // Crear socket servidor y escuchar
    int fd = conectar(port);
    if (fd == -1) {
        exit(-1);
    }

    socklen_t size = sizeof(struct sockaddr_in);
    struct sockaddr_in client;
    pthread_t tid;

    while (1) {

        int fd_cliente = accept(fd, (struct sockaddr*)&client, &size);
        if (fd_cliente == -1) {
            perror("Error en accept");
            continue;
        }

        // Crear hilo para manejar al cliente
        if (pthread_create(&tid, NULL, manejar_cliente, (void*)(intptr_t)fd_cliente) != 0) {
            perror("Error al crear hilo");
            close(fd_cliente);
            continue;
        }

    }

    close(fd);   
}
