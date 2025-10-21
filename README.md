# Trabajo realizado por:
-Jonathan Felipe Lopez Núñez
-Juan David Castañeda Cárdenas
-Omar Darío Zambrano Galindo

# Proyecto Spotify Dataset

## Dataset utilizado
[900k Spotify Dataset — Kaggle](https://www.kaggle.com/datasets/devdope/900k-spotify/data?select=spotify_dataset.csv)

---

## Instrucciones de uso

### 1. Preparación del dataset

1. Ejecuta el programa de Python **`Hash.py`** para modificar el contenido del dataset y hacerlo compatible con los demás programas del proyecto.
2. Una vez procesado, cambia el nombre del dataset a **`songs.csv`**, ya que los demás programas del repositorio esperan ese nombre.

---

### 2. Creación de índices

Después de haber modificado el dataset, ejecuta el programa **`crear_indices.c`** para generar el archivo de índices necesario para las búsquedas.

---

### 3. Ejecución de los programas principales

- El programa **`clienteSong.c`** envía los datos del artista y la canción al servidor.
- Si deseas buscar múltiples artistas, escríbelos separados por comas, por ejemplo:

  ```text
  artista1,artista2
