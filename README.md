# OpenMP: la misma cuenta, secuencial y paralela

Infraestructuras Paralelas y Distribuidas
Escuela de Ingeniería de Sistemas y Computación, Universidad del Valle
Carlos Andrés Delgado Saavedra

[![Pruebas](../../actions/workflows/pruebas.yml/badge.svg)](../../actions/workflows/pruebas.yml)

Los mismos problemas que ya se repartieron con `std::thread` y con TBB,
ahora con directivas. Cada programa trae la versión secuencial escrita y
pide la paralela; los dos imprimen el resultado al lado del tiempo, y el
resultado se compara antes que el reloj.

| Parte | Archivo | Qué directiva |
|---|---|---|
| 1 | `hadamard.cpp` | `parallel for` y `reduction`, con aceleración y eficiencia |
| 2 | `reparto.cpp` | `schedule` sobre tareas de costo desigual |
| 3 | `histograma.cpp` | La carrera de datos, `atomic`, `critical` y la reducción de un arreglo |
| 4 | `secciones.cpp` | `sections` frente a un `parallel for` con tres reducciones |

## Parte 1: el producto de Hadamard

`w[i] = u[i] * v[i]` y después la suma de `w`, con `u` en 4 y `v` en 9,
así que la suma tiene que dar `36 * n`. `secuencial` ya está. Falta
`paralelo`: el producto con `#pragma omp parallel for` y la suma con la
cláusula `reduction`, porque todos los hilos acumulan sobre la misma
variable y sin ella el número sale mal.

```bash
make hadamard
```

El programa corre la versión secuencial y después la paralela con 1, 2, 4 y
8 hilos, fijados con `omp_set_num_threads`, e imprime la aceleración y la
eficiencia de cada corrida. Con un hilo la versión paralela tarda un poco
más que la secuencial: es el costo de armar la región. De ahí en adelante la
aceleración crece menos que los hilos, y con cien millones de enteros el
motivo es el mismo de siempre: el programa hace muy poco cálculo por cada
dato que trae de memoria.

## Parte 2: `schedule`

Sesenta y cuatro tareas y cuatro hilos, con dos patrones de costo: en el
**creciente** la tarea `i` cuesta proporcional a `i` a la cuarta, y en el
**periódico** una de cada cuatro cuesta veinticinco veces lo que las otras.
`bloques` ya está, con `schedule(static)`. Faltan `turnos` y `demanda`, y
en cada una lo que cambia es la cláusula.

```bash
make reparto
```

Con `static` el último bloque del patrón creciente concentra el 77 % del
trabajo. Con `static, 1` las tareas caras del patrón periódico caen todas en
el mismo hilo. Con `dynamic` ninguno de los dos patrones estorba, porque el
hilo que se desocupa toma la siguiente iteración libre. Los seis totales
tienen que coincidir dentro de cada patrón.

## Parte 3: la carrera de datos

Un histograma de veinte millones de valores en dieciséis cajones. `secuencial`
ya está, y también `con_carrera`: varios hilos incrementan los mismos
contadores sin protección, y el total sale mal porque las escrituras se
pisan. Faltan tres formas de arreglarlo:

- `con_atomic`: cada incremento es una operación atómica.
- `con_critical`: cada incremento entra a una sección crítica.
- `con_reduccion`: cada hilo cuenta en su copia privada y las copias se
  suman al final. OpenMP reduce arreglos con
  `reduction(+ : cuentas[:CAJONES])` sobre un arreglo de C.

```bash
make histograma
```

El programa corre con cuatro hilos e imprime, para cada versión, el tiempo,
el total, el cajón cero y si coincide con la secuencial. `critical` es
cientos de veces más lenta que la secuencial y `atomic`, decenas; solo la
reducción gana, y por mucho. Proteger cada incremento serializa lo
que se acababa de repartir; contar aparte y combinar una vez es lo que hace
`reduction` con un escalar, extendido a dieciséis.

## Parte 4: `sections`

Tres recorridos distintos del mismo vector de cien millones de enteros: la
suma, el máximo y cuántos superan un umbral. `secuencial` los hace uno tras
otro. Faltan dos versiones:

- `con_secciones`: los tres recorridos a la vez, cada uno en una `section`
  de un `parallel sections`. Adentro de una sección corre un solo hilo.
- `en_una_pasada`: los tres resultados en un solo `parallel for` con tres
  reducciones, una por resultado; la del máximo usa el operador `max`.

```bash
make secciones
```

Las secciones reparten trabajos, así que con tres trabajos el techo es tres,
tenga los núcleos que tenga la máquina. La pasada única reparte datos y
escala con los núcleos. Es la misma comparación entre descomposición de
tareas y de datos, con los tres resultados iguales en las tres versiones.

## Cómo compilar y ejecutar

```bash
make hadamard      # parte 1, deja hadamard.txt
make reparto       # parte 2, deja reparto.txt
make histograma    # parte 3, deja histograma.txt
make secciones     # parte 4, deja secciones.txt
make todo          # las cuatro
```

Cada regla compila con `-fopenmp`, ejecuta y borra el ejecutable. OpenMP
viene con `g++`; si falta el compilador, `script.sh` lo instala en Debian o
Ubuntu. El número de hilos de las partes 2 y 4 se controla desde el ambiente,
sin recompilar:

```bash
OMP_NUM_THREADS=2 make secciones
```

## Qué revisa el flujo de Actions

- Parte 1: que las cinco sumas den `36 * n` y que cuatro hilos tarden menos
  que uno.
- Parte 2: que los seis totales sean los correctos; que el reparto por
  demanda no tarde más que los bloques en el patrón creciente ni más que los
  turnos en el periódico, y que los turnos no tarden más que los bloques en
  el creciente; y que el programa haya usado más de un procesador en
  promedio, medido como tiempo de CPU sobre tiempo de reloj.
- Parte 3: que `atomic`, `critical` y la reducción coincidan con la
  secuencial, que la reducción sea más rápida que `atomic`, y que el programa
  haya corrido en varios procesadores.
- Parte 4: que los tres resultados sean los mismos en las tres versiones y
  que tanto las secciones como la pasada única tarden menos que la
  secuencial.

Cada parte es un job aparte: la lista de verificaciones del commit dice cuál
quedó en verde y cuál no, y la pestaña del run trae un resumen con la salida
de cada programa y el conteo de partes en verde. Cuando una verificación de
tiempos falla, el flujo repite la corrida una vez antes de marcar rojo, y el
error queda anotado sobre el archivo de esa parte. Un push nuevo cancela el
run anterior.

Los tiempos del registro son los de un servidor compartido con cuatro
procesadores; los que valen para la discusión son los de su máquina.

## Lo que hay que poder explicar

- Por qué con un hilo la versión paralela tarda más que la secuencial, y
  por qué con ocho la eficiencia queda tan abajo.
- Qué hace cada cláusula `schedule` con las sesenta y cuatro iteraciones, y
  cuál perdió en cada patrón.
- Por qué `con_carrera` cuenta menos de veinte millones, por qué `critical`
  es tan lenta y por qué la reducción le gana incluso a la secuencial.
- Cuál es el techo de aceleración de `sections` con tres trabajos, y por
  qué la pasada única no tiene ese techo.
- Cómo se ve el mismo Hadamard con `std::thread`, con TBB y con OpenMP:
  cuánto código pide cada uno y quién decide el reparto.
