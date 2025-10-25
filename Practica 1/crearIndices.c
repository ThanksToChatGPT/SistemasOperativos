// Genera index.csv: 350 líneas, cada una con offsets (en bytes) separados por comas
// correspondientes a la primera canción de cada artista en ese bucket
// Notas:
// - Abre songs.csv en "rb" para que ftell/offset sea consistente (Windows/Linux).
// - Usa getline (requiere -D_GNU_SOURCE en GCC).
// - Parser robusto para bucket (col 0) y artist (col 1) con comillas.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <openssl/md5.h>

#define NUM_BUCKETS 350 // Número total de cubetas de hash a utilizar (0 a 349).
#define MAX_ARTIST  512 // Tamaño máximo del buffer para el nombre del artista.

//  Vector dinámico
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

// --- HASH MD5 coherente con serverSong.c ---
int hash_mod350_index(const char *artist) {
    unsigned char digest[MD5_DIGEST_LENGTH];
    char norm[MAX_ARTIST];
    normalize_artist(artist, norm, sizeof(norm));
    MD5((unsigned char*)norm, strlen(norm), digest);
    unsigned long val = 0;
    for (int i = 0; i < 3; i++) {
        val = (val << 8) | digest[i];
    }
    return (int)(val % NUM_BUCKETS);
}

int main(int argc, char **argv) {
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
