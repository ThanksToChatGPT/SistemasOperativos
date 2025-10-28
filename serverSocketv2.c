#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <openssl/md5.h>
#include <stdbool.h>
#include <time.h>

#define MAX_ARTIST 512
#define MAX_SONG 512
#define MAX_LINE 40000
#define PORT 3535
#define NUM_BUCKETS 350
#define BACKLOG 5



//---------- CREAR INDICES -----------
typedef struct {
    long   *data;                     // offsets (inicio de fila en songs.csv)
    size_t  size;                     // número de offsets
    size_t  cap;                      // capacidad reservada
    char    last_artist[MAX_ARTIST];  // último artista visto en este bucket
} BucketVec;

// iniciar el vector
static void vec_init(BucketVec *v) {
    v->data = NULL;
    v->size = 0;
    v->cap  = 0;
    v->last_artist[0] = '\0';
}

// Agrega un nuevo offset al vector, reasignando memoria si es necesario (crecimiento dinámico x2).
static void vec_push(BucketVec *v, long value) {
    if (v->size == v->cap) {
        // duplicar la capacidad o dejarla en 64 
        size_t newcap = v->cap ? v->cap * 2 : 64; 
        long *p = (long *)realloc(v->data, newcap * sizeof(long));
        if (!p) {
            fprintf(stderr, "Fallo realloc: %s\n", strerror(errno));
            exit(1);
        }
        v->data = p;
        v->cap  = newcap;
    }
    v->data[v->size++] = value;
}

//liberar la memoria dinamica del vector
static void vec_free(BucketVec *v) {
    free(v->data);
    v->data = NULL;
    v->size = v->cap = 0;
}

//  Parser: bucket (col 0) y artista (col 1) con comillas 
static int parse_line_bucket_artist(const char *line, int *bucket_out, char *artist_out) {
    const char *p = line;

    // 1) bucket = entero hasta la primera coma
    char tmp[64];
    int ti = 0;
    while (*p && *p != ',') {
        if (ti < (int)sizeof(tmp) - 1) tmp[ti++] = *p;
        p++;
    }
    if (*p != ',') return -1; // línea mal formada
    tmp[ti] = '\0';
    *bucket_out = atoi(tmp);
    p++; // saltar coma

    // 2) artista = segundo campo (con o sin comillas)
    int in_quotes = 0;
    int ai = 0;

    if (*p == '"') { in_quotes = 1; p++; }

    while (*p) {
        if (in_quotes && *p == '"') {
            p++;
            if (*p == '"') { // comilla escapada ("")
                if (ai < MAX_ARTIST - 1) artist_out[ai++] = '"';
                p++;
                continue;
            }
            if (*p == ',') { p++; break; } // cierre + coma
        } else if (!in_quotes && *p == ',') {
            p++; // fin de campo sin comillas
            break;
        }

        if (ai < MAX_ARTIST - 1) artist_out[ai++] = *p;
        p++;
    }
    artist_out[ai] = '\0';
    return 0;
}

// --- Normaliza artista: quita comillas/espacios y pasa a minúsculas ---
void normalize_artist(const char *src, char *dst, size_t dst_size) {
    // Copiar y garantizar terminación
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';

    // Quitar saltos y espacios al final
    size_t len = strlen(dst);
    while (len > 0 && (dst[len - 1] == '\n' || dst[len - 1] == '\r' || isspace((unsigned char)dst[len - 1])))
        dst[--len] = '\0';

    // Quitar espacios al inicio
    char *start = dst;
    while (*start && isspace((unsigned char)*start)) start++;

    // Eliminar comillas exteriores si existen
    if (start[0] == '"') {
        size_t slen = strlen(start);
        if (slen > 1 && start[slen - 1] == '"') {
            start[slen - 1] = '\0';
            memmove(start, start + 1, slen - 1);
        }
    }

    // Pasar a minúsculas
    for (char *p = start; *p; ++p) *p = tolower((unsigned char)*p);

    // Si se removieron caracteres al inicio, mover resultado al inicio del dst
    if (start != dst) memmove(dst, start, strlen(start) + 1);
}



int crearIndices(){
    const char *songs_csv = "songs.csv";
    const char *index_csv = "index.csv";

    FILE *fsongs = fopen(songs_csv, "rb"); // binario para offsets correctos
    if (!fsongs) {
        fprintf(stderr, "No se pudo abrir '%s': %s\n", songs_csv, strerror(errno));
        return -1;
    }
    //un arreglo de vectores
    BucketVec buckets[NUM_BUCKETS];

    for (int i = 0; i < NUM_BUCKETS; ++i) vec_init(&buckets[i]);

    // Contador total de índices (primeras canciones por artista)
    long total_indices = 0;

    // Lectura de filas con getline (dinámico, no trunca)
    char  *line = NULL;
    size_t cap  = 0;

    // 1) Leer y descartar encabezado (siempre), manteniendo consistencia de offsets
    long line_offset = ftell(fsongs);
    ssize_t nread = getline(&line, &cap, fsongs);
    if (nread < 0) {
        fprintf(stderr, "CSV vacío o error al leer encabezado.\n");
        fclose(fsongs);
        return -1;
    }
    // 2) Recorrer archivo línea por línea
    for (;;) {
        line_offset = ftell(fsongs);          // offset de la linea
        nread = getline(&line, &cap, fsongs); // fila completa
        if (nread < 0) break;                 // EOF

        int  bucket = -1;
        char artist[MAX_ARTIST];
        if (parse_line_bucket_artist(line, &bucket, artist)!= 0) continue;
        if (bucket < 0 || bucket >= NUM_BUCKETS) continue;

        BucketVec *bv = &buckets[bucket];

        // Si cambió el artista en este bucket = es la primera canción de ese artista
        if (strcmp(bv->last_artist, artist) != 0) {
            vec_push(bv, line_offset);
            strncpy(bv->last_artist, artist, MAX_ARTIST - 1);
            bv->last_artist[MAX_ARTIST - 1] = '\0';
            total_indices++; // contamos un índice más
        }
    }

    fclose(fsongs);
    free(line);

    // 3) Escribir index.csv en una sola pasada: 1 línea por bucket
    FILE *fidx = fopen(index_csv, "wb");
    if (!fidx) {
        fprintf(stderr, "No se pudo crear '%s': %s\n", index_csv, strerror(errno));
        for (int i = 0; i < NUM_BUCKETS; ++i) vec_free(&buckets[i]);
        return -1;
    }

    for (int i = 0; i < NUM_BUCKETS; ++i) {
        BucketVec *bv = &buckets[i];
        for (size_t j = 0; j < bv->size; ++j) {
            if (j > 0) fputc(',', fidx);
            fprintf(fidx, "%ld", bv->data[j]);
        }
        fputc('\n', fidx);
    }

    fclose(fidx);

    // 4) Liberar memoria
    for (int i = 0; i < NUM_BUCKETS; ++i) vec_free(&buckets[i]);

    printf("Indice generado en '%s'\n", index_csv);
    printf("Total de índices creados: %ld\n", total_indices);
    return 0;
}
//---------- FIN DE CREAR INDICES -----------

// ----------- BUSCAR CANCIÓN -----------
// --- Elimina comillas y espacios ---
void trim_quotes(char *str) {
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r' || isspace((unsigned char)str[len - 1])))
        str[--len] = '\0';
    if (str[0] == '"' && str[len - 1] == '"' && len > 1) {
        memmove(str, str + 1, len - 2);
        str[len - 2] = '\0';
    }
}
// --- Comparación sin mayúsculas/minúsculas ---
int cmp_icase(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

int hash_mod350(const char *artist) {
    char tmp[MAX_ARTIST];
    strncpy(tmp, artist, MAX_ARTIST - 1);
    tmp[MAX_ARTIST - 1] = '\0';

    // Quitar saltos y espacios al final
    size_t len = strlen(tmp);
    while (len > 0 && (tmp[len - 1] == '\n' || tmp[len - 1] == '\r' || isspace((unsigned char)tmp[len - 1])))
        tmp[--len] = '\0';

    // Quitar espacios al inicio
    char *start = tmp;
    while (*start && isspace((unsigned char)*start)) start++;

    // Eliminar comillas exteriores si existen
    if (start[0] == '"') {
        size_t slen = strlen(start);
        if (slen > 1 && start[slen - 1] == '"') {
            start[slen - 1] = '\0';
            memmove(start, start + 1, slen - 1);
        }
    }

    // Pasar a minúsculas para normalizar
    for (char *p = start; *p; ++p) *p = tolower((unsigned char)*p);

    // Calcular MD5 sobre la cadena normalizada
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5((unsigned char*)start, strlen(start), digest);

    unsigned long val = 0;
    for (int i = 0; i < 3; i++) {
        val = (val << 8) | digest[i];
    }
    return (int)(val % NUM_BUCKETS);
}

static int parse_line_artist_song(const char *line, char *artist_out, char *song_out) {
    if (!line || !artist_out || !song_out) return -1;
    const char *p = line;
    char tmp[64];
    int ti = 0;

    // ======== 1. SACAR EL BUCKET ========
    while (*p && *p != ',') {
        if (ti < (int)sizeof(tmp) - 1) tmp[ti++] = *p;
        p++;
    }
    if (*p != ',') return -1;
    tmp[ti] = '\0';
    p++; // saltar la coma

    // ======== 2. SACAR EL ARTISTA ========
    int in_quotes = 0;
    int ai = 0;
    if (*p == '"') { in_quotes = 1; p++; }

    while (*p) {
        if (in_quotes && *p == '"') {
            p++;
            if (*p == '"') { // doble comilla -> una sola
                if (ai < MAX_ARTIST - 1) artist_out[ai++] = '"';
                p++;
                continue;
            }
            if (*p == ',') { p++; break; }
            break;
        } else if (!in_quotes && *p == ',') {
            p++;
            break;
        }
        if (ai < MAX_ARTIST - 1) artist_out[ai++] = *p;
        p++;
    }
    artist_out[ai] = '\0';

    // convertir artista a minúsculas
    for (int i = 0; artist_out[i]; i++)
        artist_out[i] = tolower((unsigned char)artist_out[i]);

    // ======== 3. SACAR LA CANCIÓN ========
    int si = 0;
    while (*p && *p != ',') {
        if (si < MAX_SONG - 1) song_out[si++] = *p;
        p++;
    }
    song_out[si] = '\0';

    // convertir canción a minúsculas
    for (int i = 0; song_out[i]; i++)
        song_out[i] = tolower((unsigned char)song_out[i]);

    return 0;
}

int buscar_cancion(const char *artist, const char *song, char *resultado) {
    int bucket = hash_mod350(artist);
    FILE *findex = fopen("index.csv", "r");

    if (!findex) {
        sprintf(resultado, "Error: no se pudo abrir index.csv");
        return 0;
    }

    // Saltar líneas hasta el bucket correspondiente
    char *line = NULL;
    size_t cap = 0;
    for (int i = 0; i <= bucket; i++) {
        if (getline(&line, &cap, findex) < 0) {
            sprintf(resultado, "NA\nBucket %d no encontrado", bucket);
            fclose(findex);
            return 0;
        }
    }

    fclose(findex);

    // Parsear offsets
    long offsets[4096];
    int count = 0;
    char *tok = strtok(line, ",");
    while (tok && count < 4096) {
        offsets[count++] = atol(tok);
        tok = strtok(NULL, ",");
    }

    if (count == 0) {
        sprintf(resultado, "No hay artistas en el bucket %d.", bucket);
        free(line);
        return 0;
    }

    FILE *fsongs = fopen("songs.csv", "r");
    if (!fsongs) {
        sprintf(resultado, "Error: no se pudo abrir songs.csv");
        free(line);
        return 0;
    }

    char row[MAX_LINE];
    int found_artist = 0;

    for (int i = 0; i < count; i++) {
        fseek(fsongs, offsets[i], SEEK_SET);

        while (fgets(row, sizeof(row), fsongs)) {
            char temp[MAX_LINE];
            strcpy(temp, row);

            char a[MAX_ARTIST];
            char s[MAX_SONG];
            parse_line_artist_song(row, a, s);

            trim_quotes(a);
            trim_quotes(s);

            if(!cmp_icase(a, artist)){
                break;
            }else{
                if (cmp_icase(s, song)) {
                    strcpy(resultado, row); // copia registro completo
                    fclose(fsongs);
                    free(line);
                    return 1; //  encontrada
                }
            }
        }
    }

    fclose(fsongs);
    free(line);

    if (!found_artist){
        sprintf(resultado, "NA\nArtista '%s' no encontrado (bucket %d).", artist, bucket);
    }else
        sprintf(resultado, "NA\nCanción '%s' no encontrada para '%s'.", song, artist);
    return 0;
}
// ----------- FIN DE BUSCAR CANCIÓN -----------
// ----------- BUSCAR CANCIONES DE ARTISTA(S) -----------
int buscar_canciones_por_artista(const char *artist, char *resultado) {
    int bucket = hash_mod350(artist);
    FILE *findex = fopen("index.csv", "r");

    if (!findex) {
        sprintf(resultado, "Error: no se pudo abrir index.csv\n");
        return 0;
    }

    char *line = NULL;
    size_t cap = 0;
    for (int i = 0; i <= bucket; i++) {
        if (getline(&line, &cap, findex) < 0) {
            sprintf(resultado, "NA\nBucket %d no encontrado", bucket);
            fclose(findex);
            return 0;
        }
    }
    fclose(findex);

    long offsets[4096];
    int count = 0;
    char *tok = strtok(line, ",");
    while (tok && count < 4096) {
        offsets[count++] = atol(tok);
        tok = strtok(NULL, ",");
    }

    if (count == 0) {
        sprintf(resultado, "No hay artistas en el bucket %d.", bucket);
        free(line);
        return 0;
    }

    FILE *fsongs = fopen("songs.csv", "r");
    if (!fsongs) {
        sprintf(resultado, "Error: no se pudo abrir songs.csv");
        free(line);
        return 0;
    }

    char row[MAX_LINE];
    int found = 0;

    for (int i = 0; i < count; i++) {
        fseek(fsongs, offsets[i], SEEK_SET);

        while (fgets(row, sizeof(row), fsongs)) {
            char a[MAX_ARTIST];
            char s[MAX_SONG];
            parse_line_artist_song(row, a, s);

            trim_quotes(a);
            trim_quotes(s);

            if (!cmp_icase(a, artist)) {
                break; 
            }

            strcat(resultado, s);
            strcat(resultado, "\n");
            found = 1;
        }
    }

    fclose(fsongs);
    free(line);

    if (!found) {
        sprintf(resultado, "NA\nArtista '%s' no encontrado (bucket %d).", artist, bucket);
        return 0;
    }

    return 1;
}

// ----------- AGREGAR CANCIÓN -----------
int agregar_cancion(const char *artist, const char *song, char *resultado) {
    FILE *fsongs = fopen("songs.csv", "a");
    if (!fsongs) {
        sprintf(resultado, "Error: no se pudo abrir songs.csv");
        return 0;
    }

    fprintf(fsongs, "%d,%s,%s,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,\n", 
            hash_mod350(artist), artist, song);
    fclose(fsongs);

    crearIndices();

    sprintf(resultado, "Canción '%s' del artista '%s' agregada exitosamente.", song, artist);
    return 1;
}
// ----------- FIN DE AGREGAR CANCIÓN -----------


int main() {
    int fd, r;
    struct sockaddr_in server;
    double tiempo_ejecucion;
    clock_t inicio, fin;

    // Para TCP se debe usar (SOCK_STREAM).
    // Crear socket UDP
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd ==-1) {
        perror("Error al crear socket");
        exit(-1);
    }

    // Configurar estructura servidor
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;
    bzero(&server.sin_zero, 8);


    // Hacer bind
    r = bind(fd, (struct sockaddr*)&server, sizeof(struct sockaddr));
    if (r == -1) {
        perror("Error en bind");
        close(fd);
        exit(-1);
    }


    r = listen(fd, BACKLOG);
    if (r == -1) {
        perror("Error en listen");
        close(fd);
        exit(-1);
    }

    // Aceptar conexión
    socklen_t size = sizeof(struct sockaddr_in);
    struct sockaddr_in client;
    int fd2 = accept(fd, (struct sockaddr*)&client, &size);
    if (fd2 == -1) {
        perror("Error en accept");
        close(fd);
        exit(-1);
    }
    
    bool running = true;
    while (running) {
        printf("\nEsperando petición del cliente...\n");

        char buffer[MAX_ARTIST + MAX_SONG + 10];

        r = recv(fd2, buffer, sizeof(buffer) - 1, 0);
        if (r == -1) {
            perror("Error en recv");
            running = false;  
            break;
        } else if (r == 0) {
            printf("Cliente desconectado.\n");
            running = false;  
            break;
        }

        buffer[r] = '\0';
        printf("Mensaje recibido.\n");

        int opcion = 0;
        char artist[MAX_ARTIST];
        char song[MAX_SONG];
        char resultado[MAX_LINE];
        inicio = clock();
        //Recepción y procesamiento del mensaje.
        sscanf(buffer, "@%d@", &opcion);

        switch (opcion) {
            case 1: {
                sscanf(buffer, "@%*d@,@%[^@]@,@%[^@]@", artist, song);
                printf("Petición -> Buscar canción\n");
                memset(resultado, 0, sizeof(resultado));
                buscar_cancion(artist, song, resultado);
                break;
            }

            case 2: {
                sscanf(buffer, "@%*d@,@%[^@]@", artist);
                printf("Petición -> Buscar canciones del artista\n");
                memset(resultado, 0, sizeof(resultado));
                buscar_canciones_por_artista(artist, resultado);
                break;
            }

            case 3: {
                sscanf(buffer, "@%*d@,@%[^@]@,@%[^@]@", artist, song);
                printf("Petición -> Agregar canción\n");
                memset(resultado, 0, sizeof(resultado));
                agregar_cancion(artist, song, resultado);
                break;
            }
        }
        fin = clock();
        //Enviar respuesta al cliente.
        r = send(fd2, resultado, strlen(resultado), 0);
        if (r == -1) {
            perror("Error en send");
            running = false;
        }
        tiempo_ejecucion = ((double)(fin-inicio))/CLOCKS_PER_SEC;
        printf("Timepo de ejecicion: %.10f", tiempo_ejecucion);
    }
    printf("Cerrando servidor...\n");
    close(fd2);
    close(fd);

    return 0;

}