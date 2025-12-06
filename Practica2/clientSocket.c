#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>


int main() {
    int fd, r, opcion;
    bool exitProgram = false;
    struct sockaddr_in cliente;
    char artist[512], song[512], message[1500], buffer[40000];
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
     
    while(!exitProgram  ){
        memset(artist, 0, sizeof(artist));
        memset(song, 0, sizeof(song));
        memset(message, 0, sizeof(message));
        memset(buffer, 0, sizeof(buffer));
        
        printf("Digite el número de la acción que desea realizar:\n1. Buscar canción\n2. Buscar canciones de un artista\n3. Añadir una nueva canción de un artista\n4. Salir\n\n");
        scanf("%d", &opcion);
        getchar();
        printf("\n");

        switch (opcion) {
            case 1:
                printf("\nDigite el nombre del Artista: ");
                scanf(" %[^\n]", artist);
                getchar();

                printf("Digite el nombre de la Canción: ");
                scanf(" %[^\n]", song);
                getchar();

                snprintf(message, sizeof(message), "@1@,@%s@,@%s@", artist, song);
                r = send(fd, message, strlen(message), 0);
                if (r == -1) {
                    perror("error en send");
                    exitProgram=true;
                }
                printf("\nMensaje enviado al servidor.\n");
                
                memset(buffer, 0, sizeof(buffer));
                r = recv(fd, buffer, sizeof(buffer) - 1, 0);
                if (r == -1) {
                    perror("error en recv");
                    exitProgram=true;
                } else if (r == 0) {
                    printf("El servidor cerró la conexión.\n");
                    exitProgram=true;
                }

                buffer[r] = '\0';
                printf("\nRespuesta del servidor:\n%s\n", buffer);
                break;

            case 2:   
                printf("\nDigite el nombre del Artista: ");
                scanf(" %[^\n]", artist);
                getchar();

                snprintf(message, sizeof(message), "@2@,@%s@", artist);

                r = send(fd, message, strlen(message), 0);
                if (r == -1) {
                    perror("error en send");
                    exitProgram=true;
                }
                printf("\nMensaje enviado al servidor.\n");

                memset(buffer, 0, sizeof(buffer));
                r = recv(fd, buffer, sizeof(buffer) - 1, 0); 
                if (r == -1) {
                    perror("error en recv");
                    exitProgram=true;
                } else if (r == 0) {
                    printf("El servidor cerró la conexión.\n");
                    exitProgram=true;
                }

                buffer[r] = '\0'; 
                printf("\nRespuesta del servidor:\n%s\n", buffer);
                break;

            case 3:
                printf("\nDigite el nombre del Artista que desea agregar: ");
                scanf(" %[^\n]", artist);
                getchar();

                printf("Digite el nombre de la Canción que desea agregar: ");
                scanf(" %[^\n]", song);
                getchar();

                snprintf(message, sizeof(message), "@3@,@%s@,@%s@", artist, song);
                r = send(fd, message, strlen(message), 0);
                if (r == -1) {
                    perror("error en send");
                    exitProgram=true;
                }
                printf("\nMensaje enviado al servidor.\n");

                memset(buffer, 0, sizeof(buffer));
                r = recv(fd, buffer, sizeof(buffer) - 1, 0);
                if (r == -1) {
                    perror("error en recv");
                    exitProgram=true;
                } else if (r == 0) {
                    printf("El servidor cerró la conexión.\n");
                    exitProgram=true;
                }

                buffer[r] = '\0';
                printf("\nRespuesta del servidor:\n%s\n", buffer);
                break;

            case 4:
                printf("\nSaliendo del programa...\n");
                exitProgram=true;
                break;

            default:
                printf("\nOpción no válida.\n");
                break;
        }
    }

    close(fd);
    printf("\nPrograma finalizado.\n");
    return 0;
}