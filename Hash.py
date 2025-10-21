# Programa para la modificacón del Dataset spotify_dataset.csv

import pandas as pd
import hashlib

archivo = "spotify_dataset.csv"

# Función para pasar cadeda de caractares a los seis primeros
# digitos del md5 hash
def short_md5(texto):
    if pd.isna(texto):  # manejar valores vacíos o NaN
        texto = ""
    texto = texto.strip().lower()  # normaliza texto
    return hashlib.md5(texto.encode()).hexdigest()[:6].upper()


df = pd.read_csv(archivo)

# Verificar si la clumna que se modifica existe
if "Artist(s)" not in df.columns:
    raise ValueError("No se encontró la columna 'Artist(s)' en el CSV.")

# Se crean las columnas
# h_artist: con el hash de md5 versión corta
# bucket: (h_artist)%350
df["h_artist"] = df["Artist(s)"].apply(short_md5)
df["bucket"] = df["h_artist"].apply(lambda h: int(h, 16) % 350)

# Reordenar csv para facilitar su uso
cols = ["bucket"] + [c for c in df.columns if c != "bucket"]
df = df[cols]

# Guardar Cambios hechos
df.to_csv(archivo, index=False)

print(f"Archivo '{archivo}' actualizado con columnas 'h_artist' y 'bucket' (mod 350).")
