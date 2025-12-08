

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <time.h>

#define SHM_NAME "/meminfo_shm"
#define SEM_NAME "/meminfo_sem"

#define MAX_AGENTES 8
#define IP_LEN 32

typedef struct {
    char ip[IP_LEN];

    float cpu_usage;
    float user_pct;
    float system_pct;
    float idle_pct;

    int mem_used;
    int mem_free;
    int swap_total;
    int swap_free;

    int tiene_cpu;                
    int tiene_mem;            
    time_t ultima_actualizacion;  
} InfoAgente;


typedef struct {
    InfoAgente agentes[MAX_AGENTES];
    int usados;   
} MemCompartida;


MemCompartida *mem;
sem_t *sem;

int obtener_indice_agente(const char *ip) {
   
    for (int i = 0; i < mem->usados; i++) {
        if (strcmp(mem->agentes[i].ip, ip) == 0) {
            return i;
        }
    }

  
    if (mem->usados < MAX_AGENTES) {
        int idx = mem->usados++;
        memset(&mem->agentes[idx], 0, sizeof(InfoAgente));
        strncpy(mem->agentes[idx].ip, ip, IP_LEN - 1);
        mem->agentes[idx].ip[IP_LEN - 1] = '\0';
        return idx;
    }


    return -1;
}


void procesar_linea(char *linea) {
    char tipo[8];
    char ip[IP_LEN];

    /*
     Formato esperado:
        CPU;IP;total;user;sys;idle
        MEM;IP;used;free;swap_total;swap_free
    */
    if (strncmp(linea, "CPU;", 4) == 0) {
        float cpu_usage, userp, sysp, idlep;

        sscanf(linea, "%7[^;];%31[^;];%f;%f;%f;%f",
               tipo, ip, &cpu_usage, &userp, &sysp, &idlep);

        int idx = obtener_indice_agente(ip);
        if (idx == -1) return;

        InfoAgente *a = &mem->agentes[idx];
        a->cpu_usage = cpu_usage;
        a->user_pct = userp;
        a->system_pct = sysp;
        a->idle_pct = idlep;
        a->tiene_cpu = 1;                       
        a->ultima_actualizacion = time(NULL);   
    }

    else if (strncmp(linea, "MEM;", 4) == 0) {
        int used, free, swap_total, swap_free;

        sscanf(linea, "%7[^;];%31[^;];%d;%d;%d;%d",
               tipo, ip, &used, &free, &swap_total, &swap_free);

        int idx = obtener_indice_agente(ip);
        if (idx == -1) return;

        InfoAgente *a = &mem->agentes[idx];
        a->mem_used = used;
        a->mem_free = free;
        a->swap_total = swap_total;
        a->swap_free = swap_free;
        a->tiene_mem = 1;                       
        a->ultima_actualizacion = time(NULL);   
    }

    // Despertar impresor
    sem_post(sem);
}

void *hilo_cliente(void *arg) {
    int fd = (intptr_t)arg;
    char buffer[256];

    while (1) {
        int r = recv(fd, buffer, sizeof(buffer)-1, 0);
        if (r <= 0) {
            close(fd);
            pthread_exit(NULL);
        }

        buffer[r] = '\0';

      
        char *line = strtok(buffer, "\n");
        while (line) {
            procesar_linea(line);
            line = strtok(NULL, "\n");
        }
    }
}

int main(int argc, char *argv[]) {

    if (argc != 2) {
        printf("Uso: %s <puerto>\n", argv[0]);
        exit(1);
    }

    int puerto = atoi(argv[1]);


    sem = sem_open(SEM_NAME, O_CREAT, 0666, 0);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }


    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    ftruncate(shm_fd, sizeof(MemCompartida));

    mem = mmap(NULL, sizeof(MemCompartida),
               PROT_READ | PROT_WRITE, MAP_SHARED,
               shm_fd, 0);
    if (mem == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }


    mem->usados = 0;

    int fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in srv;
    srv.sin_family = AF_INET;
    srv.sin_port = htons(puerto);
    srv.sin_addr.s_addr = INADDR_ANY;
    bzero(&srv.sin_zero, 8);

    if (bind(fd, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
        perror("bind");
        exit(1);
    }

    listen(fd, 10);
    printf("Collector esperando agentes en el puerto %d...\n", puerto);

    while (1) {
        struct sockaddr_in cli;
        socklen_t sz = sizeof(cli);

        int fd_cli = accept(fd, (struct sockaddr *)&cli, &sz);
        if (fd_cli < 0) continue;

        /
        int r = send(fd_cli, "Servidor conectado", 18, 0);
        if (r <= 0) {
            close(fd_cli);
            continue;
        }


        pthread_t tid;
        pthread_create(&tid, NULL, hilo_cliente, (void*)(intptr_t)fd_cli);
        pthread_detach(tid);
    }

    return 0;
}
