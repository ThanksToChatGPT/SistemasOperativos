
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SHM_NAME "/meminfo_shm"
#define SEM_NAME "/meminfo_sem"

#define MAX_AGENTES 8
#define IP_LEN 32
#define UMBRAL_STALE 4    

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
    int shm_fd;
    MemCompartida *mem;

    // Abrir semáforo ya creado por el collector
    sem_t *sem = sem_open(SEM_NAME, 0);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    // Abrir memoria compartida
    shm_fd = shm_open(SHM_NAME, O_RDONLY, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    mem = mmap(NULL, sizeof(MemCompartida),
               PROT_READ, MAP_SHARED, shm_fd, 0);
    if (mem == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    while (1) {
        // Esperar hasta que collector publique algo
        sem_wait(sem);

        system("clear");
        printf("%-15s %-6s %-6s %-6s %-6s %-8s %-8s %-10s %-10s\n",
               "IP", "CPU%", "User%", "Sys%", "Idle%",
               "MemUsed", "MemFree", "SwapTotal", "SwapFree");

        time_t ahora = time(NULL);

        for (int i = 0; i < mem->usados; i++) {
            InfoAgente *a = &mem->agentes[i];

            int viejo = (a->ultima_actualizacion == 0) ||
                        (difftime(ahora, a->ultima_actualizacion) > UMBRAL_STALE);

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