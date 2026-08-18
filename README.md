# OpenMP: la misma cuenta, secuencial y paralela

Infraestructuras Paralelas y Distribuidas
Escuela de Ingeniería de Sistemas y Computación, Universidad del Valle
Carlos Andrés Delgado Saavedra

El mismo problema resuelto dos veces, para poder comparar. La versión
secuencial marca el punto de partida; la paralela con OpenMP dice cuánto se
gana repartiendo, y a partir de qué punto deja de ganarse.

## El problema

El producto de Hadamard de dos vectores multiplica posición a posición:
`w[i] = u[i] * v[i]` para cada `i` entre `0` y `n`. Sobre eso:

1. Llenar `u` y `v` con un valor constante pequeño, menor que diez.
2. Calcular el producto elemento a elemento.
3. Sumar el vector resultante.
4. Imprimir el resultado y el tiempo de cada función en milisegundos.

## Los dos archivos

- `secuencial.cpp`: la versión de un solo hilo, sin ninguna directiva.
- `paralelo.cpp`: la misma solución con OpenMP. El llenado y el producto salen
  con `#pragma omp parallel for`; la suma necesita `reduction`, porque todos
  los hilos acumulan sobre la misma variable.

## Cómo compilar y ejecutar

```bash
make secuencial   # compila, ejecuta y deja la salida en secuencial.txt
make paralelo     # compila con -O3 -ffast-math -fopenmp y deja paralelo.txt
```

El número de hilos se controla desde el ambiente, sin recompilar:

```bash
OMP_NUM_THREADS=4 make paralelo
```

## Qué revisa el flujo de Actions

Que las dos versiones compilen y corran, y que dejen su archivo de salida con
contenido. La implementación se revisa en clase.

## Lo que hay que poder explicar

Con cuatro hilos la aceleración no llega a cuatro. Parte se va en crear y
sincronizar los hilos, y parte en que la operación hace muy poco cálculo por
cada dato que trae de memoria. Comparar los dos archivos de salida con
distintos valores de `OMP_NUM_THREADS` es la mitad del ejercicio.
