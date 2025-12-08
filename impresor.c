#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

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
} InfoAgente;

typedef struct {
    InfoAgente ultimo;
} MemCompartida;


int main() {
    int shm_id;
    MemCompartida *mem;

    //Semaforo para leer
    sem_t *sem = sem_open(SEM_NAME, 0);
    if (sem == SEM_FAILED) {
        perror("Error abriendo semaforo");
        exit(1);
    }

    shm_id = shm_open(SHM_NAME, O_CREAT | O_RDONLY, 0666);
    if (shm_id == -1) {
        perror("Error abriendo memoria compartida");
        exit(1);
    }

    mem = mmap(NULL, sizeof(MemCompartida), PROT_READ, MAP_SHARED, shm_id, 0);
    if (mem == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    while (1) {
        //Esperando que se escriba contenido en la memoria
        sem_wait(sem);

        system("clear");
        printf("%-15s %-6s %-6s %-6s %-6s %-8s %-8s %-10s %-10s\n",
       "IP", "CPU%", "User%", "Sys%", "Idle%", "MemUsed","MemFree", "SwapTotal", "SwapFree");

        InfoAgente *a = &mem->ultimo;

        printf("%-15s %-6.1f %-6.1f %-6.1f %-6.1f %-8d %-8d %-10d %-10d\n",
       a->ip,a->cpu_usage, a->user_pct, a->system_pct, a->idle_pct, a->mem_used, a->mem_free, a->swap_total, a->swap_free);

        fflush(stdout);
    }

    return 0;
}
