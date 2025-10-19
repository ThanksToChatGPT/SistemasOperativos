#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <openssl/md5.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#define NUM_BUCKETS 350
#define MAX_LINE 4096
#define MAX_ARTIST 512
#define MAX_SONG 512
#define FIFO_CLIENT_TO_SERVER "/tmp/client_to_server"
#define FIFO_SERVER_TO_CLIENT "/tmp/server_to_client"

// --- HASH MD5 → bucket (0–349) ---
// Nueva versión: normaliza el nombre del artista (quita comillas/espacios y pasa a minúsculas)
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

// --- BUSCAR CANCIÓN ---
int buscar_cancion(const char *artist, const char *song, char *resultado) {
    int bucket = hash_mod350(artist);
    FILE *findex = fopen("index.csv", "r");
    if (!findex) {
        sprintf(resultado, "❌ Error: no se pudo abrir index.csv");
        return 0;
    }

    // Saltar líneas hasta el bucket correspondiente
    char *line = NULL;
    size_t cap = 0;
    for (int i = 0; i <= bucket; i++) {
        if (getline(&line, &cap, findex) < 0) {
            sprintf(resultado, "❌ Bucket %d no encontrado", bucket);
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
        sprintf(resultado, "⚠️  No hay artistas en el bucket %d.", bucket);
        free(line);
        return 0;
    }

    FILE *fsongs = fopen("songs.csv", "r");
    if (!fsongs) {
        sprintf(resultado, "❌ Error: no se pudo abrir songs.csv");
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

            char *bucket_str = strtok(temp, ",");
            char *a = strtok(NULL, ",");
            char *s = strtok(NULL, ",");

            if (!a || !s) break;
            trim_quotes(a);
            trim_quotes(s);

            if (!found_artist && cmp_icase(a, artist)) {
                found_artist = 1;
            }

            if (found_artist) {
                if (!cmp_icase(a, artist)) break; // terminó artista
                if (cmp_icase(s, song)) {
                    strcpy(resultado, row); // copia registro completo
                    fclose(fsongs);
                    free(line);
                    return 1; // ✅ encontrada
                }
            }
        }

        if (found_artist) break;
    }

    fclose(fsongs);
    free(line);

    if (!found_artist)
        sprintf(resultado, "🚫 Artista '%s' no encontrado (bucket %d).", artist, bucket);
    else
        sprintf(resultado, "🚫 Canción '%s' no encontrada para '%s'.", song, artist);
    return 0;
}

int main() {
    mkfifo(FIFO_CLIENT_TO_SERVER, 0666);
    mkfifo(FIFO_SERVER_TO_CLIENT, 0666);

    printf("🖥️ Servidor listo. Esperando peticiones...\n");

    while (1) {
        int fd_in = open(FIFO_CLIENT_TO_SERVER, O_RDONLY);
        char buffer[MAX_ARTIST + MAX_SONG + 5];
        if (read(fd_in, buffer, sizeof(buffer)) <= 0) {
            close(fd_in);
            continue;
        }
        close(fd_in);

        char artist[MAX_ARTIST], song[MAX_SONG];
        sscanf(buffer, "%[^|]|%[^\n]", artist, song);

        printf("🎵 Buscando '%s' - '%s'\n", artist, song);

        char resultado[MAX_LINE];
        buscar_cancion(artist, song, resultado);

        int fd_out = open(FIFO_SERVER_TO_CLIENT, O_WRONLY);
        write(fd_out, resultado, strlen(resultado) + 1);
        close(fd_out);

        printf("✅ Respuesta enviada al cliente.\n\n");
    }

    unlink(FIFO_CLIENT_TO_SERVER);
    unlink(FIFO_SERVER_TO_CLIENT);
    return 0;
}
