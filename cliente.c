#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

//./cliente 127.0.0.1 3535 2.2.2.2

struct prev_cpu {
    unsigned long total;
    unsigned long user;
    unsigned long system;
    unsigned long idle;
};


int conectar(char *ip_server, char *puerto) {
    int fd;
    struct sockaddr_in servidor;

    // Crear socket TCP
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("error al crear socket");
        return -1;
    }

    // Configurar dirección del servidor
    servidor.sin_family = AF_INET;
    servidor.sin_port = htons(atoi(puerto));
    servidor.sin_addr.s_addr = inet_addr(ip_server);
    bzero(&servidor.sin_zero, 8);

    // Conectarse al servidor
    if (connect(fd, (struct sockaddr*)&servidor, sizeof(struct sockaddr_in)) == -1) {
        perror("error en connect");
        close(fd);
        return -1;
    }

    return fd;
}


void enviar_memoria(int socket_fd, char *ip_logica_agente) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) {
        perror("fopen");
        return;
    }
    
    char line[256];
    long mem_total_kb = 0;
    long mem_available_kb = 0;
    long mem_free_kb = 0;
    long swap_total_kb = 0;
    long swap_free_kb = 0;

    
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "MemTotal: %ld kB", &mem_total_kb) == 1)
            continue;
        if (sscanf(line, "MemAvailable: %ld kB", &mem_available_kb) == 1)
            continue;
        if (sscanf(line, "MemFree: %ld kB", &mem_free_kb) == 1)
            continue;
        if (sscanf(line, "SwapTotal: %ld kB", &swap_total_kb) == 1)
            continue;
        if (sscanf(line, "SwapFree: %ld kB", &swap_free_kb) == 1)
            continue;
    }

    long mem_used_mb = (mem_total_kb - mem_available_kb) / 1024;
    long mem_free_mb = mem_free_kb / 1024;
    long swap_total_mb = swap_total_kb / 1024;
    long swap_free_mb = swap_free_kb / 1024;

    char buffer[100];
    sprintf(buffer, "MEM;%s;%ld;%ld;%ld;%ld\n", ip_logica_agente, 
        mem_used_mb, mem_free_mb, swap_total_mb, swap_free_mb);
    
    printf("Enviando: %s", buffer);
    
    if(send(socket_fd, buffer, strlen(buffer), 0) == -1) {
        perror("error en send");
        close(socket_fd);
        exit(-1);
    }
    
    
}

void enviar_cpu(int socket_fd, char *ip_logica_agente, struct prev_cpu *prev) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) {
        perror("fopen");
        return;
    }
    
    char cpu_label[5];
    unsigned long user, nice, system, idle, iowait, irq, softirq, steal;
    
    if (fscanf(f, "%4s %lu %lu %lu %lu %lu %lu %lu %lu",
               cpu_label, &user, &nice, &system, &idle,
               &iowait, &irq, &softirq, &steal) == 9) {
    } else {
        fprintf(stderr, "No se pudo leer la línea de cpu\n");
    }
    fclose(f);

    long total = user + nice + system + idle + iowait + irq + softirq + steal;
    long cpu_total = total - prev->total;
    long cpu_idle = idle - prev->idle;

    long cpu_usage = 100 * (cpu_total - cpu_idle) / (cpu_total);
    long user_pct = 100 * (user - prev->user) / (cpu_total);
    long system_pct = 100 * (system - prev->system) / (cpu_total);
    long idle_pct = 100 * (cpu_idle) / (cpu_total);
    
    char buffer[100];
    sprintf(buffer, "CPU;%s;%ld;%ld;%ld;%ld\n", ip_logica_agente, 
        cpu_usage, user_pct, system_pct, idle_pct);
    printf("Enviando: %s", buffer);

    prev->total = total;
    prev->user = user;
    prev->system = system;
    prev->idle = idle;

    if (send(socket_fd, buffer, strlen(buffer), 0) == -1) {
        perror("error en send");
        close(socket_fd);
        exit(-1);
    }

}


int main(int argc, char *argv[]) {
    char *ip_server = argv[1];
    char *puerto = argv[2];
    char *ip = argv[3];
    
    // Conectarse al servidor
    int fd = conectar(ip_server, puerto);
    if (fd == -1) {
        exit(-1);
    }


    char buffer[20];
    int r = recv(fd, buffer, 20, 0);
    if (r == -1) {
        perror("error en recv");
        close(fd);
        exit(-1);
    }
    buffer[r] = '\0';
    printf("%s\n", buffer);


    r = send(fd, "Agente conectado", 17, 0);
    if (r == -1) {
        perror("error en send");
        close(fd);
        exit(-1);
    }
    struct prev_cpu prev = {0};

    while(1) {
        enviar_memoria(fd, ip);
        enviar_cpu(fd, ip, &prev);
        sleep(2);
    }

    close(fd);
    return 0;
}
