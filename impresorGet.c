#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define SHM_NAME "/meminfo_shm"
#define SEM_NAME "/meminfo_sem"
#define UMBRAL_STALE 4 

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


int main() {
    int shm_id;
    key_t key = 12345;

    //Semaforo para leer
    sem_t *sem = sem_open(SEM_NAME, 0);
    if (sem == SEM_FAILED) {
        perror("Error abriendo semaforo");
        exit(1);
    }

    shm_id = shmget(key, sizeof(MemCompartida), 0666);
    if (shm_id < 0) {
        perror("Error al crear shm_id");
        exit(1);
    }

    MemCompartida *mem = (MemCompartida *) shmat(shm_id, NULL, 0);
    if (mem == (void *) -1) {
        perror("Error al crear apuntador");
        exit(1);
    }

    while (1) {
        //Esperando que se escriba contenido en la memoria
        sem_wait(sem);

        system("clear");
        printf("%-15s %-6s %-6s %-6s %-6s %-8s %-8s %-10s %-10s\n",
       "IP", "CPU%", "User%", "Sys%", "Idle%", "MemUsed","MemFree", "SwapTotal", "SwapFree");

        time_t ahora = time(NULL);

        for (int i = 0; i < mem->usados; i++) {
            InfoAgente *a = &mem->agentes[i];

            int viejo = (a->ultima_actualizacion == 0) || (difftime(ahora, a->ultima_actualizacion) > UMBRAL_STALE);
            // IP
            printf("%-15s ", a->ip);

            // CPU
            if (viejo || !a->tiene_cpu)
                printf("%-6s %-6s %-6s %-6s ",
                    "---", "---", "---", "---");
            else
                printf("%-6.1f %-6.1f %-6.1f %-6.1f ",
                    a->cpu_usage, a->user_pct, a->system_pct, a->idle_pct);

            // MEM
            if (viejo || !a->tiene_mem)
                printf("%-8s %-8s %-10s %-10s\n",
                    "---", "---", "---", "---");
            else
                printf("%-8d %-8d %-10d %-10d\n",
                    a->mem_used, a->mem_free,
                    a->swap_total, a->swap_free);
        }
        fflush(stdout);
    }

    return 0;
}
