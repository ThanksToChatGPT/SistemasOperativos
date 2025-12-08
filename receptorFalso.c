#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define SHM_NAME "/meminfo_shm"
#define SEM_NAME "/meminfo_sem"

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
    int shm_fd;
    MemCompartida *mem;

    // Crear semáforo
    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0666, 0);
    if (sem == SEM_FAILED) {
        perror("Error creando semáforo");
        exit(1);
    }

    // Crear memoria compartida
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Error abriendo shm");
        exit(1);
    }

    ftruncate(shm_fd, sizeof(MemCompartida));

    mem = mmap(NULL, sizeof(MemCompartida),
               PROT_READ | PROT_WRITE,
               MAP_SHARED,
               shm_fd,
               0);

    if (mem == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    printf("Mini receptor.\n");

    strcpy(mem->ultimo.ip, "192.168.1.50");
    mem->ultimo.cpu_usage = 72.5;
    mem->ultimo.user_pct = 55.0;
    mem->ultimo.system_pct = 10.2;
    mem->ultimo.idle_pct = 34.8;
    mem->ultimo.mem_used = 3072;
    mem->ultimo.mem_free = 512;
    mem->ultimo.swap_total = 4096;
    mem->ultimo.swap_free = 2048;

    // Despertar al impresor
    sem_post(sem);

    printf("Datos enviados.\n");

    return 0;
}
