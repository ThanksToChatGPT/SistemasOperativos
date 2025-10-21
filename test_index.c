// print_offsets.c
// Imprime (bucket, artist) de CADA offset listado en la línea del bucket de index.csv.
//
// Uso:
//   gcc print_offsets.c -o print_offsets -D_GNU_SOURCE
//   ./print_offsets songs.csv index.csv 189
//
// Notas:
// - Usa getline para leer la línea completa de offsets.
// - Abre songs.csv en "rb" y usa getline para leer cada fila completa.
// - Parser robusto para bucket (col 0) y artist (col 1) con comillas.

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define NUM_BUCKETS 350
#define MAX_ARTIST  512

// Parser de bucket (col 0) y artista (col 1) con comillas
static int parse_line_bucket_artist(const char *line, int *bucket_out, char *artist_out) {
    const char *p = line;

    //SACAR EL BUCKET
    // buffer
    char tmp[64];
    // puntero provisional
    int ti = 0;
    // mientras que el caracter exista y sea diferente a ,
    while (*p && *p != ',') {
        //si aun hay espacio en el buffer, copiar e ir al siguiente caracter
        if (ti < (int)sizeof(tmp) - 1) tmp[ti++] = *p;
        p++;
    }
    // si despues de copiar hasta la primera coma, el siguiente caracter es ",", hay error
    if (*p != ',') return -1;
    // fin de la cadena
    tmp[ti] = '\0';
    //guardar el NUMERO a *bucket _out
    *bucket_out = atoi(tmp);
    //preparar para el siguiente campo (artista)
    p++;

    // SACAR EL ARTISTA
    //bandera para saber si estamos entre comillas
    int in_quotes = 0;
    //indice para el artista
    int ai = 0;
    //si inicia con comillas, se activa la bandera
    if (*p == '"') { in_quotes = 1; p++; }
    //mientras exista un caracter
    while (*p) {
        //si hay una comilla despues de otra comilla, puede ser la comilla de cierre
        if (in_quotes && *p == '"') {
            p++;
            //si hay 2 comillas seguidas, se convierte a 1
            if (*p == '"') { // "" => "
                if (ai < MAX_ARTIST - 1) artist_out[ai++] = '"';
                p++;
                continue;
            }
            //si despues de las comillas hay una coma, se termina el bucle
            if (*p == ',') { p++; break; }

            break;
            //si no hay comillas y encuentra , termina
        } else if (!in_quotes && *p == ',') {
            p++;
            break;
        }
        //copiar si no es una coma o comilla
        if (ai < MAX_ARTIST - 1) artist_out[ai++] = *p;
        p++;
    }
    artist_out[ai] = '\0';
    return 0;
}

// Quita el salto de linea o retorno de carro y lo remplaza por \0
static void rstrip_eol(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r')) {
        s[--n] = '\0';
    }
}

int main(int argc, char **argv) {
    const char *songs_csv = (argc > 1) ? argv[1] : "songs.csv";
    const char *index_csv = (argc > 2) ? argv[2] : "index.csv";
    int target_bucket     = (argc > 3) ? atoi(argv[3]) : 1;

    if (target_bucket < 0 || target_bucket >= NUM_BUCKETS) {
        fprintf(stderr, "Bucket fuera de rango [0..%d]\n", NUM_BUCKETS-1);
        return 1;
    }

    // 1) Abrir index.csv y leer la línea del bucket deseado
    FILE *fidx = fopen(index_csv, "rb");
    if (!fidx) {
        fprintf(stderr, "No se pudo abrir '%s': %s\n", index_csv, strerror(errno));
        return -1;
    }

    char  *idxline = NULL;
    size_t cap     = 0;
    ssize_t nread  = 0;

    int current = 0;
    //traer toda la linea del bucket a idxline
    while ((nread = getline(&idxline, &cap, fidx)) >= 0) {
        if (current == target_bucket) break;
        current++;
    }
    fclose(fidx);

    if (current != target_bucket || nread < 0) {
        fprintf(stderr, "No se encontró la línea del bucket %d en '%s'\n", target_bucket, index_csv);
        free(idxline);
        return -1;
    }

    // 2) Abrir songs.csv en binario y preparar buffer de fila
    FILE *fsongs = fopen(songs_csv, "rb");
    if (!fsongs) {
        fprintf(stderr, "No se pudo abrir '%s': %s\n", songs_csv, strerror(errno));
        free(idxline);
        return -1;
    }

    char  *row  = NULL;
    size_t rc   = 0;

    // 3) Tokenizar offsets de la línea (separadores: coma y CR/LF)
    //    Usamos strtok sobre una copia de la línea para no perder el original si lo quisieras reutilizar.
    rstrip_eol(idxline);
    char *line_copy = strdup(idxline);
    if (!line_copy) {
        fprintf(stderr, "Sin memoria para duplicar línea de índices\n");
        fclose(fsongs);
        free(idxline);
        return -1;
    }

    printf("=== Bucket %d ===\n", target_bucket);
    //tomar el primer offset
    for (char *tok = strtok(line_copy, ","); tok != NULL;tok = strtok(NULL, ",")) {

        // omitir tokens vacíos, por si acaso
        while (*tok == ' ' || *tok == '\t') {
            tok++;
            printf("Espacio o tabulacion dentro del offset");
        }
        if (*tok == '\0') continue;

        errno = 0;
        //convertir el offset a long
        long offset = strtol(tok, NULL, 10);
        if (errno != 0) {
            fprintf(stderr, "Offset inválido en índice: '%s'\n", tok);
            continue;
        }

        // Saltar al offset y leer esa fila COMPLETA
        if (fseek(fsongs, offset, SEEK_SET) != 0) {
            fprintf(stderr, "fseek falló al offset %ld\n", offset);
            continue;
        }
        ssize_t rn = getline(&row, &rc, fsongs);
        if (rn < 0) {
            fprintf(stderr, "No se pudo leer fila en offset %ld\n", offset);
            continue;
        }

        int  bline = -1;
        char artist[MAX_ARTIST];
        if (parse_line_bucket_artist(row, &bline, artist)!= 0) {
            fprintf(stderr, "Fila mal formada en offset %ld\n", offset);
            continue;
        }

        printf("bucket=%d  artist=%s\n", bline, artist);
    }

    free(line_copy);
    free(idxline);
    free(row);
    fclose(fsongs);
    return 0;
}
