# Documentación de apoyo: OpenMP: la misma cuenta, secuencial y paralela

Este documento acompaña al README. Hay una sección por parte, en el mismo
orden y con el mismo nombre, y cada una trae las directivas y funciones que
esa parte necesita, un ejemplo completo sobre un problema vecino con su salida
real, lo que suele fallar al escribirlo y, al final, los enlaces a la
documentación. Los ejemplos se compilan con `g++ -std=c++17 -O2 -fopenmp`,
las mismas banderas del `Makefile`, y se copian a un archivo aparte para
correrlos tal cual: no resuelven lo que pide el README, muestran las piezas
con las que se escribe. Las salidas son de un procesador con seis núcleos y
doce hilos; en otra máquina cambian los tiempos y el orden en que hablan los
hilos, no las cuentas.

## Parte 1: el producto de Hadamard

### Lo que se usa

- `#pragma omp parallel for [cláusulas]`: abre una región paralela y reparte
  entre sus hilos las iteraciones del `for` que viene en la línea siguiente.
  El bucle tiene que tener la forma `for (i = inicio; i < fin; i++)`, con una
  variable entera y un límite que no cambie adentro; la variable del bucle es
  privada de cada hilo. Al cerrar el `for` hay una barrera: nadie sigue hasta
  que todos terminen.
- `reduction(op : lista)`: cada hilo recibe una copia privada de cada
  variable de la lista, inicializada con el neutro del operador (0 para `+`,
  1 para `*`, el menor valor del tipo para `max`), acumula en ella, y al
  terminar el bucle las copias se combinan con el valor que la variable tenía
  afuera. Va en la misma línea del `parallel for`. Admite varias variables,
  `reduction(+ : a, b)`, y se puede repetir con otro operador.
- `void omp_set_num_threads(int n)`: fija cuántos hilos tendrán las regiones
  paralelas que se abran después. Manda sobre `OMP_NUM_THREADS`.
- `int omp_get_num_threads()`: cuántos hilos hay en el equipo de la región
  desde donde se llama. Afuera de toda región devuelve 1.
- `int omp_get_thread_num()`: el número del hilo que llama, entre 0 y el
  total menos uno. El 0 es el que abrió la región.
- `int omp_get_max_threads()`: cuántos hilos tendría la próxima región, sin
  abrirla.
- `double omp_get_wtime()`: segundos de reloj de pared, como `double`, desde
  un punto fijo; se restan dos lecturas y se multiplica por 1.000 para tener
  milisegundos. Los programas del repositorio usan
  `std::chrono::high_resolution_clock`, que sirve igual.
- `OMP_NUM_THREADS=n`: variable de ambiente que fija los hilos de todo el
  programa sin recompilar: `OMP_NUM_THREADS=2 ./programa`. Una llamada a
  `omp_set_num_threads` dentro del programa la deja sin efecto.
- Compartidas y privadas por omisión: lo declarado antes de la región es una
  sola variable que ven todos los hilos; lo declarado adentro del bloque y la
  variable de control del `for` son una copia por hilo. `private(x)` y
  `shared(x)` lo cambian, y `default(none)` hace que el compilador rechace
  toda variable que no esté listada explícitamente: es la forma más rápida de
  encontrar una compartida que tenía que ser privada.

### Ejemplo

Dos programas. El primero muestra quién es cada hilo y cómo se reparten seis
iteraciones entre tres hilos; el segundo aproxima pi integrando
`4 / (1 + x^2)` entre 0 y 1 con la regla del punto medio, primero en un hilo y
después con 1, 2 y 4, e imprime aceleración y eficiencia.

`hilos.cpp`:

```cpp
// Cuántos hilos hay, quién es cada uno y a quién le toca cada iteración.
#include <cstdio>
#include <omp.h>

int main() {
  omp_set_num_threads(3);
  printf("hilos disponibles: %d\n", omp_get_max_threads());

#pragma omp parallel
  printf("hilo %d de %d\n", omp_get_thread_num(), omp_get_num_threads());

#pragma omp parallel for
  for (int i = 0; i < 6; i++)
    printf("iteracion %d en el hilo %d\n", i, omp_get_thread_num());

  printf("afuera de la region hay %d hilo\n", omp_get_num_threads());
  return 0;
}
```

```bash
g++ -std=c++17 -O2 -fopenmp -o hilos hilos.cpp
./hilos
```

```
hilos disponibles: 3
hilo 0 de 3
hilo 1 de 3
hilo 2 de 3
iteracion 2 en el hilo 1
iteracion 0 en el hilo 0
iteracion 1 en el hilo 0
iteracion 3 en el hilo 1
iteracion 4 en el hilo 2
iteracion 5 en el hilo 2
afuera de la region hay 1 hilo
```

El orden de las líneas cambia de una corrida a otra: cada hilo imprime cuando
le toca el procesador, no en fila. Lo que no cambia es el reparto: `libgomp`,
el runtime de `g++`, usa `static` cuando no se escribe `schedule`, y cada
hilo recibe un bloque contiguo, el 0 las iteraciones 0 y 1, el 1 las 2 y 3,
el 2 las 4 y 5.

`pi.cpp`:

```cpp
// Aproxima pi integrando 4 / (1 + x^2) entre 0 y 1 con la regla del punto
// medio: un parallel for con reduction sobre la suma. Corre con 1, 2 y 4
// hilos e imprime aceleración y eficiencia.
#include <clocale>
#include <cstdio>
#include <omp.h>

const long PASOS = 400000000;

double pi_secuencial() {
  double h = 1.0 / PASOS, suma = 0;
  for (long i = 0; i < PASOS; i++) {
    double x = (i + 0.5) * h;
    suma += 4.0 / (1.0 + x * x);
  }
  return suma * h;
}

double pi_paralelo() {
  double h = 1.0 / PASOS, suma = 0;
#pragma omp parallel for reduction(+ : suma)
  for (long i = 0; i < PASOS; i++) {
    double x = (i + 0.5) * h;  // declarada adentro: una copia por hilo
    suma += 4.0 / (1.0 + x * x);
  }
  return suma * h;
}

int main() {
  setlocale(LC_NUMERIC, "");  // separador decimal de la configuración regional
  double t0 = omp_get_wtime();
  double base = pi_secuencial();
  double ms_base = (omp_get_wtime() - t0) * 1000;
  printf("secuencial %.1f ms pi %.12f\n", ms_base, base);

  int hilos[] = {1, 2, 4};
  for (int k : hilos) {
    omp_set_num_threads(k);
    t0 = omp_get_wtime();
    double pi = pi_paralelo();
    double ms = (omp_get_wtime() - t0) * 1000;
    printf("hilos %d %.1f ms pi %.12f aceleracion %.2f eficiencia %.2f\n",
           k, ms, pi, ms_base / ms, ms_base / ms / k);
  }
  return 0;
}
```

```bash
g++ -std=c++17 -O2 -fopenmp -o pi pi.cpp
./pi
```

```
secuencial 520,4 ms pi 3,141592653590
hilos 1 520,5 ms pi 3,141592653590 aceleracion 1,00 eficiencia 1,00
hilos 2 261,1 ms pi 3,141592653590 aceleracion 1,99 eficiencia 1,00
hilos 4 137,5 ms pi 3,141592653590 aceleracion 3,79 eficiencia 0,95
```

Con un hilo la versión paralela tarda lo mismo que la secuencial: abrir la
región cuesta microsegundos y el bucle, medio segundo. Con dos y cuatro hilos
la aceleración va pegada al número de hilos, 1,99 y 3,79, porque cada
iteración hace unas pocas operaciones de punto flotante sobre valores que ya
tiene en registros y no trae nada de memoria: el trabajo se reparte y nada lo
frena. Un bucle que recorre un vector grande y hace una sola operación por
elemento no escala así: ahí manda el ancho de banda de memoria, que no se
multiplica con los hilos, y la eficiencia cae mucho antes de los cuatro
hilos. Las doce cifras impresas de pi coinciden en las cuatro corridas: la
reducción suma los parciales en otro orden, y con este integrando la
diferencia queda por debajo del último decimal que se imprime.

### Lo que suele fallar

- **La suma sale mal y el producto bien.** Falta `reduction(+ : suma)` en la
  directiva del segundo bucle: los hilos escriben la misma variable y se
  pisan, y sale un número menor que el correcto y distinto en cada corrida.
  En el ejemplo de pi, sin la cláusula y con cuatro hilos, 0,719 en una
  corrida y 0,568 en la siguiente; con dos hilos, 1,855; con `-O0`, 0,557,
  0,562 y 0,966. Con un acumulador entero y `-O2` el síntoma engaña: cada
  hilo acumula en un registro y escribe su parcial al final, así que queda
  el de un solo hilo, la cuarta parte exacta con cuatro hilos e igual en
  todas las corridas. El contador de la parte 3 lo muestra.
- **La suma sale multiplicada por el número de hilos.** Se escribió
  `#pragma omp parallel` sin `for`: cada hilo ejecuta el bucle completo y la
  reducción suma las copias. En el ejemplo de pi, 6,28 con dos hilos y 12,57
  con cuatro, y el tiempo no baja de los 500 ms porque nadie repartió nada.
- **`error: loop nest expected before ...`** Entre la directiva y el `for`
  hay otra cosa, una declaración o una asignación. El `for` va en la línea
  siguiente al `#pragma`.
- **`error: 's' has not been declared`** en la línea del `reduction`. La
  variable de la cláusula tiene que existir antes de la región; una declarada
  adentro del bucle ya es privada y no hay nada que reducir.
- **`undefined reference to 'omp_set_num_threads'`** al enlazar, o el
  programa corre en un solo hilo sin quejarse. Falta `-fopenmp`: sin la
  bandera `g++` ignora las directivas y no enlaza `libgomp`. Con `-Wall`
  avisa `ignoring #pragma omp parallel`.
- **Cuatro hilos no bajan del 85 % del tiempo de uno.** Uno de los dos bucles
  quedó sin directiva, o la máquina tiene menos procesadores de los que se
  cree: `nproc` los cuenta. En una máquina virtual o en WSL2 con dos
  procesadores, la corrida de ocho hilos se reparte esos dos.

### Enlaces

- [parallel Construct, OpenMP 5.1](https://www.openmp.org/spec-html/5.1/openmpse14.html): qué hace la directiva `parallel`, cuántos hilos abre y qué cláusulas admite.
- [Worksharing-Loop Construct, OpenMP 5.1](https://www.openmp.org/spec-html/5.1/openmpsu48.html): la forma que tiene que tener el `for` y la definición de las cláusulas `schedule`, `collapse` y `nowait`.
- [Reduction Clauses and Directives, OpenMP 5.1](https://www.openmp.org/spec-html/5.1/openmpsu117.html): cómo se inicializan las copias privadas y con qué operadores se combinan.
- [Data-Sharing Attribute Rules, OpenMP 5.1](https://www.openmp.org/spec-html/5.1/openmpsu113.html): qué es compartido y qué es privado cuando no se dice nada.
- [Runtime Library Routines, libgomp](https://gcc.gnu.org/onlinedocs/libgomp/Runtime-Library-Routines.html): `omp_set_num_threads`, `omp_get_thread_num`, `omp_get_wtime` y las demás funciones de `omp.h`, con prototipo y descripción.
- [OpenMP Reference Guides](https://www.openmp.org/resources/refguides/): la tarjeta de referencia en PDF, con toda la sintaxis de C/C++ en pocas páginas.

## Parte 2: `schedule`

### Lo que se usa

Todas son cláusulas de `#pragma omp parallel for` y cambian solo quién
ejecuta cada iteración; el bucle y la reducción se escriben igual.

- `schedule(static)`: divide las iteraciones en tantos bloques contiguos
  como hilos, de tamaño parecido, y le entrega uno a cada hilo antes de
  empezar. No hay ninguna sincronización durante el bucle. Es lo que `libgomp`
  aplica cuando no se escribe `schedule`.
- `schedule(static, k)`: bloques de `k` iteraciones repartidos por turnos.
  El hilo 0 recibe las iteraciones 0 a k − 1, el 1 las k a 2k − 1, y así
  hasta que se acaban los hilos y se vuelve al 0. Con `k = 1` y cuatro hilos,
  el hilo h hace las iteraciones h, h + 4, h + 8 y así. El reparto también
  queda decidido antes de empezar.
- `schedule(dynamic)` y `schedule(dynamic, k)`: los bloques, de `k`
  iteraciones (1 si no se dice), se entregan a medida que los hilos los
  piden: el que termina toma el siguiente libre. Cada bloque cuesta una
  sincronización, así que con iteraciones muy cortas conviene subir `k`.
- `schedule(guided[, k])`: como `dynamic`, pero los bloques empiezan grandes
  y van bajando hasta `k`. Sirve cuando el desbalance está al final del
  bucle.
- `schedule(runtime)`: la decisión se toma al ejecutar, con la variable de
  ambiente `OMP_SCHEDULE` (`OMP_SCHEDULE="dynamic,4" ./programa`) o con
  `omp_set_schedule(omp_sched_t tipo, int bloque)`, donde el tipo es
  `omp_sched_static`, `omp_sched_dynamic` o `omp_sched_guided`. Permite
  probar los repartos sin recompilar.
- `omp_get_thread_num()` dentro del bucle: la forma directa de ver quién
  tomó cada iteración: se guarda en un arreglo indexado por `i` y se imprime
  al final.

### Ejemplo

Dieciséis iteraciones y cuatro hilos, con un costo que baja: la iteración 0
es la más cara y la 15 la más barata. El programa prueba cuatro repartos con
`schedule(runtime)` y `omp_set_schedule`, para no escribir el bucle cuatro
veces, e imprime qué hilo se llevó cada iteración, el tiempo y el total.

`quien_toma.cpp`:

```cpp
// Qué hilo se lleva cada iteración con cada schedule, y cuánto tarda el
// reparto cuando las primeras iteraciones son las caras.
#include <clocale>
#include <cstdio>
#include <omp.h>

const int N = 16;
const int HILOS = 4;

// Trabajo sintético: la iteración i cuesta proporcional a (N - i) al cubo,
// así que la 0 es la más cara y la 15 la más barata.
long costo(int i) {
  long vueltas = (long)(N - i) * (N - i) * (N - i) * 20000;
  long s = 0;
  for (long k = 0; k < vueltas; k++) s += k & 1;
  return s;
}

int main() {
  setlocale(LC_NUMERIC, "");
  omp_set_num_threads(HILOS);
  struct { const char *nombre; omp_sched_t tipo; int bloque; } planes[] = {
      {"static", omp_sched_static, 0},  {"static,2", omp_sched_static, 2},
      {"dynamic,1", omp_sched_dynamic, 1}, {"guided", omp_sched_guided, 0}};

  for (auto &p : planes) {
    omp_set_schedule(p.tipo, p.bloque);  // lo que schedule(runtime) va a usar
    int dueno[N];
    long total = 0;
    double t0 = omp_get_wtime();
#pragma omp parallel for schedule(runtime) reduction(+ : total)
    for (int i = 0; i < N; i++) {
      dueno[i] = omp_get_thread_num();
      total += costo(i);
    }
    double ms = (omp_get_wtime() - t0) * 1000;
    printf("%-10s ", p.nombre);
    for (int i = 0; i < N; i++) printf("%d", dueno[i]);
    printf("  %.1f ms  total %ld\n", ms, total);
  }
  return 0;
}
```

```bash
g++ -std=c++17 -O2 -fopenmp -o quien_toma quien_toma.cpp
./quien_toma
```

```
static     0000111122223333  63,4 ms  total 184960000
static,2   0011223300112233  45,0 ms  total 184960000
dynamic,1  1023320320020222  25,5 ms  total 184960000
guided     1111000333222222  68,0 ms  total 184960000
```

El mapa se lee posición por posición, la iteración 0 a la izquierda. En
`static` el hilo 0 se queda con las cuatro iteraciones más caras, 0 a 3, y
los otros tres lo esperan. En `static,2` los bloques de dos se van turnando y
el desbalance baja. En `dynamic,1` el mapa es distinto en cada corrida,
porque se arma sobre la marcha, y es el más rápido. `guided` entrega los
bloques grandes primero, justo donde están las iteraciones caras, y con este
patrón sale peor que `static`. Los cuatro totales coinciden: el reparto
cambia el tiempo, no la cuenta.

### Lo que suele fallar

- **`error: expected ',' or ')' before numeric constant`** en la directiva.
  Se escribió `schedule(static 1)` sin la coma entre el tipo y el tamaño del
  bloque.
- **El reparto por demanda tarda lo mismo que un solo hilo y el mapa es
  `0000000000000000`.** El tamaño de bloque es igual o mayor que el número de
  iteraciones, como `schedule(dynamic, 64)` con 64 tareas: el primer hilo
  que pide se lleva todo.
- **Los totales no coinciden entre repartos.** Al copiar la directiva de
  `bloques` se perdió `reduction(+ : total)`. El flujo compara los totales
  con los exactos y marca rojo aunque los tiempos sean buenos.
- **`OMP_NUM_THREADS` no cambia nada.** El programa llama a
  `omp_set_num_threads` y la llamada manda sobre la variable de ambiente.
  Para probar con otro número de hilos hay que cambiar la constante o quitar
  la llamada.
- **El primer reparto que corre sale más lento de lo que debería.** La
  primera región paralela del programa crea los hilos y ese costo se cobra
  una sola vez; después el equipo se queda vivo y las regiones siguientes lo
  vuelven a usar. Para comparar repartos con justicia se corre una región de
  calentamiento antes o se repite la medición.

### Enlaces

- [Worksharing-Loop Construct, OpenMP 5.1](https://www.openmp.org/spec-html/5.1/openmpsu48.html): la definición de cada tipo de `schedule` y qué pasa con las iteraciones que sobran en `static`.
- [OMP_SCHEDULE, OpenMP 5.1](https://www.openmp.org/spec-html/5.1/openmpse58.html): la sintaxis de la variable de ambiente que lee `schedule(runtime)`.
- [omp_set_schedule, OpenMP 5.1](https://www.openmp.org/spec-html/5.1/openmpsu130.html): la función que fija el reparto en tiempo de ejecución y los valores de `omp_sched_t`.
- [OMP_SCHEDULE, libgomp](https://gcc.gnu.org/onlinedocs/libgomp/OMP_005fSCHEDULE.html): cómo la interpreta el runtime de GCC, con ejemplos de valores.
- [Environment Variables, libgomp](https://gcc.gnu.org/onlinedocs/libgomp/Environment-Variables.html): todas las variables que lee `libgomp`, incluidas `OMP_NUM_THREADS`, `OMP_SCHEDULE` y `OMP_DISPLAY_ENV`.
- [omp_get_thread_num, OpenMP 5.1](https://www.openmp.org/spec-html/5.1/openmpsu123.html): la función con la que se marca qué hilo tomó cada iteración.

## Parte 3: la carrera de datos

### Lo que se usa

Hay carrera de datos cuando dos hilos tocan la misma variable al mismo
tiempo y al menos uno escribe. `cuentas[c]++` son tres pasos, leer, sumar uno
y escribir; si dos hilos leen el mismo valor antes de que alguno escriba, uno
de los dos incrementos se pierde. Se ve como un total menor que el correcto,
distinto en cada corrida, y muchas veces como un programa más lento que el
secuencial, porque la línea de caché con los contadores va rebotando entre
núcleos. En el histograma del repositorio, que ya trae `con_carrera` escrita,
cuatro hilos dieron en esta máquina 14.447.565 en una corrida y 14.386.998
en la siguiente, de 20.000.000, y tardaron 74,3 ms contra 10,0 ms de la
secuencial.

- `#pragma omp atomic`: la sentencia que sigue, que tiene que ser una
  actualización simple de una variable escalar (`x++`, `x--`, `x += e`,
  `x = x op e`), se hace como una sola operación indivisible: nadie se mete
  entre leer y escribir. Cubre exactamente una sentencia, sin llaves ni `if`.
  Hay variantes `atomic read`, `atomic write` y `atomic capture` para leer,
  escribir, o leer y actualizar en un solo paso.
- `#pragma omp critical [(nombre)]`: el bloque que sigue lo ejecuta un solo
  hilo a la vez; los demás esperan en la entrada. Adentro cabe cualquier
  código. Todas las secciones críticas sin nombre del programa comparten un
  mismo candado; con nombre, solo se excluyen entre sí las que tienen el
  mismo.
- `reduction(+ : arreglo[:n])`: reducción sobre una sección de arreglo,
  `arreglo[inicio:longitud]`, con inicio 0 si se omite: cada hilo recibe una
  copia privada del arreglo entero, inicializada en 0, y al final las copias
  se suman elemento por elemento. Funciona sobre arreglos de C
  (`long c[16]`) y sobre punteros (`long *p = v.data()`), no sobre un
  `std::vector` escrito directamente.
- `#pragma omp parallel` seguido de `#pragma omp for`: la región y el
  reparto del bucle en dos directivas. Lo que se declara entre las dos es
  privado por hilo, así que ahí cabe la copia local que después se combina
  en una sección crítica: es lo que `reduction` hace por dentro.
- `private(x)` y `firstprivate(x)`: una copia por hilo, sin inicializar o
  inicializada con el valor que la variable tenía afuera.

### Ejemplo

Dos programas. El primero cuenta cuántos valores pares hay en un vector de
veinte millones con un solo contador compartido: sin protección, con
`atomic`, con `critical` y con `reduction`. El segundo suma las columnas de
una matriz guardada por filas, con `reduction` sobre una sección de arreglo
y, al lado, la misma cuenta hecha a mano con una copia por hilo y una sección
crítica.

`pares.cpp`:

```cpp
// Cuántos valores pares hay en un vector, contados por varios hilos sobre un
// solo contador: primero sin protección, después con atomic, con critical y
// con reduction.
#include <clocale>
#include <cstdio>
#include <omp.h>
#include <vector>

using namespace std;
const size_t N = 20000000;

long pares_sin_proteger(const vector<int> &v) {
  long pares = 0;
#pragma omp parallel for
  for (size_t i = 0; i < v.size(); i++)
    if (v[i] % 2 == 0) pares++;  // varios hilos escriben la misma variable
  return pares;
}

long pares_atomic(const vector<int> &v) {
  long pares = 0;
#pragma omp parallel for
  for (size_t i = 0; i < v.size(); i++)
    if (v[i] % 2 == 0) {
#pragma omp atomic
      pares++;
    }
  return pares;
}

long pares_critical(const vector<int> &v) {
  long pares = 0;
#pragma omp parallel for
  for (size_t i = 0; i < v.size(); i++)
    if (v[i] % 2 == 0) {
#pragma omp critical
      pares++;
    }
  return pares;
}

long pares_reduction(const vector<int> &v) {
  long pares = 0;
#pragma omp parallel for reduction(+ : pares)
  for (size_t i = 0; i < v.size(); i++)
    if (v[i] % 2 == 0) pares++;  // cada hilo cuenta en su copia de pares
  return pares;
}

int main() {
  setlocale(LC_NUMERIC, "");
  vector<int> v(N);
  for (size_t i = 0; i < N; i++) v[i] = (int)(i * 7919 % 1000);
  long base = 0;
  for (int x : v) if (x % 2 == 0) base++;
  printf("secuencial pares %ld\n", base);

  struct { const char *nombre; long (*f)(const vector<int> &); } versiones[] = {
      {"carrera", pares_sin_proteger}, {"atomic", pares_atomic},
      {"critical", pares_critical}, {"reduccion", pares_reduction}};
  for (auto &ver : versiones) {
    double t0 = omp_get_wtime();
    long p = ver.f(v);
    printf("%-9s %7.1f ms pares %ld %s\n", ver.nombre, (omp_get_wtime() - t0) * 1000,
           p, p == base ? "coincide" : "DIFIERE");
  }
  return 0;
}
```

```bash
g++ -std=c++17 -O2 -fopenmp -o pares pares.cpp
OMP_NUM_THREADS=4 ./pares
```

```
secuencial pares 10000000
carrera       3,4 ms pares 2500000 DIFIERE
atomic       88,1 ms pares 10000000 coincide
critical    666,9 ms pares 10000000 coincide
reduccion     3,6 ms pares 10000000 coincide
```

`carrera` cuenta la cuarta parte: con `-O2` cada hilo lleva su contador en
un registro y lo escribe al terminar, y sobrevive el último. `atomic` y
`critical` cuentan bien y cuestan veinticuatro y más de ciento ochenta veces
lo que la reducción: diez millones de incrementos protegidos son diez millones
de turnos para la misma dirección de memoria, y `critical` además toma y
suelta un candado en cada uno. `reduction` no protege nada: cada hilo cuenta
en su copia y hay una sola suma de cuatro números al final.

`columnas.cpp`:

```cpp
// Suma por columnas de una matriz guardada por filas. Cada hilo toma filas
// y acumula en su propia copia de `col`; OpenMP suma las copias al final.
// La segunda versión hace a mano lo que la cláusula hace sola.
#include <cstdio>
#include <omp.h>
#include <vector>

using namespace std;
const int FILAS = 4000000, COLS = 6;

void sumar_columnas(const vector<int> &m, long *col) {
#pragma omp parallel for reduction(+ : col[:COLS])
  for (int f = 0; f < FILAS; f++)
    for (int c = 0; c < COLS; c++) col[c] += m[f * COLS + c];
}

void sumar_columnas_a_mano(const vector<int> &m, long *col) {
#pragma omp parallel
  {
    long mia[COLS] = {0};  // declarada adentro: una copia por hilo
#pragma omp for
    for (int f = 0; f < FILAS; f++)
      for (int c = 0; c < COLS; c++) mia[c] += m[f * COLS + c];
#pragma omp critical
    for (int c = 0; c < COLS; c++) col[c] += mia[c];  // una vez por hilo
  }
}

int main() {
  vector<int> m(FILAS * COLS);
  for (int i = 0; i < FILAS * COLS; i++) m[i] = i % 7;

  long col[COLS] = {0}, col2[COLS] = {0};
  sumar_columnas(m, col);
  sumar_columnas_a_mano(m, col2);
  for (int c = 0; c < COLS; c++)
    printf("columna %d: reduccion %ld, a mano %ld\n", c, col[c], col2[c]);
  return 0;
}
```

```bash
g++ -std=c++17 -O2 -fopenmp -o columnas columnas.cpp
OMP_NUM_THREADS=4 ./columnas
```

```
columna 0: reduccion 12000003, a mano 12000003
columna 1: reduccion 12000000, a mano 12000000
columna 2: reduccion 11999997, a mano 11999997
columna 3: reduccion 11999994, a mano 11999994
columna 4: reduccion 11999998, a mano 11999998
columna 5: reduccion 12000002, a mano 12000002
```

Las dos versiones dan lo mismo. En `sumar_columnas_a_mano` se ve lo que la
cláusula esconde: `mia` está declarada adentro de la región, así que hay una por hilo;
`omp for` reparte las filas; y el `critical` del final se ejecuta una vez por
hilo, cuatro veces en total, no una por elemento.

### Lo que suele fallar

- **`error: 'cuentas' does not have pointer or array type`** en la línea del
  `reduction`. La sección de arreglo se pidió sobre un `std::vector`. Se
  declara un arreglo de C del tamaño de los cajones, o se toma el puntero con
  `long *p = cuentas.data()` y se reduce `p[:CAJONES]`.
- **`error: expected primary-expression before 'if'`** debajo de
  `#pragma omp atomic`. La directiva cubre una sola sentencia de
  actualización; un `if`, un bloque con llaves o dos sentencias no le sirven.
  El `if` va afuera y el `#pragma` adentro, justo antes del incremento.
- **Con `critical` un contador queda bien y otro no.** Dos sentencias en la
  misma línea después del `#pragma omp critical` sin llaves: solo la primera
  queda protegida. En una prueba con `a++; b++;` en una línea, `a` dio
  1.000.000 y `b` 998.821. Con llaves entran las dos.
- **La reducción no le gana a `atomic`.** `atomic` y `critical` tardan más
  que la secuencial por diseño; la que tiene que ganar es la reducción. Si no
  gana, adentro del bucle quedó un `atomic` o un `critical` por elemento, o
  la combinación de las copias se metió dentro del bucle en lugar de hacerse
  una vez al terminar.
- **Helgrind y `-fsanitize=thread` marcan carreras en la versión con
  `reduction`.** No conocen la sincronización interna de `libgomp` y reportan
  falsos positivos en la combinación de las copias. Aquí la carrera se
  detecta con la versión secuencial al lado; el programa lo hace con
  `coincide` y `DIFIERE`.

### Enlaces

- [atomic Construct, OpenMP 5.1](https://www.openmp.org/spec-html/5.1/openmpsu105.html): las formas de sentencia que admite `atomic` y sus variantes `read`, `write`, `update` y `capture`.
- [critical Construct, OpenMP 5.1](https://www.openmp.org/spec-html/5.1/openmpsu99.html): la sección crítica, los nombres y qué pasa con las que no lo tienen.
- [Array Sections, OpenMP 5.1](https://www.openmp.org/spec-html/5.1/openmpsu21.html): la sintaxis `arreglo[inicio:longitud]` que usa la reducción de arreglos.
- [Reduction Clauses and Directives, OpenMP 5.1](https://www.openmp.org/spec-html/5.1/openmpsu117.html): la cláusula `reduction` con secciones de arreglo y los operadores permitidos.
- [Data-Sharing Attribute Rules, OpenMP 5.1](https://www.openmp.org/spec-html/5.1/openmpsu113.html): por qué una variable declarada dentro de la región es privada y una de afuera es compartida.
- [std::vector::data, cppreference](https://en.cppreference.com/w/cpp/container/vector/data): el puntero al arreglo interno del vector; es lo que acepta la sección de arreglo.

## Parte 4: `sections`

### Lo que se usa

- `#pragma omp parallel sections` seguido de un bloque con varias
  `#pragma omp section`: abre la región y reparte los bloques de sección
  entre los hilos: cada sección la ejecuta un hilo completo, de principio a
  fin, y al cerrar la llave exterior hay una barrera. Si hay más secciones
  que hilos, un hilo hace varias, una tras otra; si hay más hilos que
  secciones, los que sobran esperan en la barrera. Con tres secciones el
  techo de aceleración es tres.
- `#pragma omp sections` a secas: lo mismo dentro de una región `parallel`
  ya abierta. Sin región abierta compila y corre, pero en un solo hilo.
- `#pragma omp section`: marca el comienzo de cada bloque. La primera
  sección puede ir sin la directiva; las demás no.
- `reduction(max : m)` y `reduction(min : m)`: reducciones con los
  operadores de máximo y mínimo. La copia privada empieza en el menor o el
  mayor valor del tipo, y el valor que traía la variable de afuera también
  participa en la combinación.
- Varias cláusulas `reduction` en la misma directiva,
  `reduction(+ : a) reduction(max : b)`, o varias variables con el mismo
  operador, `reduction(+ : a, b)`. Cada una es independiente de las otras.
- `omp_get_thread_num()` dentro de una sección: para comprobar que cada
  sección corrió en un hilo distinto, o en cuál cayeron dos cuando faltaron
  hilos.

### Ejemplo

Dos programas. El primero pone en secciones tres trabajos que no comparten
nada: ordenar una copia de un vector de cuatro millones, sumar otro de sesenta
millones y contar cuántos primos hay en un tercero de un millón, por división.
Los corre uno tras otro y después en un `parallel sections`; cada trabajo
imprime en qué hilo corrió y cuánto tardó, y al final va el total. El segundo
saca el menor y el mayor de un vector en una sola pasada, con dos reducciones
en la misma directiva.

`trabajos.cpp`:

```cpp
// Tres trabajos que no tienen nada que ver entre sí, cada uno en una sección:
// ordenar una copia de un vector, sumar otro y contar cuántos primos hay en
// un tercero. Cada sección dice en qué hilo corrió y cuánto tardó; el total de
// la región lo marca la más larga.
#include <algorithm>
#include <clocale>
#include <cstdio>
#include <omp.h>
#include <vector>

using namespace std;

long ordenar(vector<int> v) {  // recibe una copia y ordena la copia
  sort(v.begin(), v.end());
  return v[v.size() / 2];  // la mediana
}

long sumar(const vector<int> &v) {
  long s = 0;
  for (int x : v) s += x;
  return s;
}

long primos_en(const vector<int> &v) {  // por división, sin criba
  long cuantos = 0;
  for (int k : v) {
    bool primo = k >= 2;
    for (int d = 2; d * d <= k; d++)
      if (k % d == 0) { primo = false; break; }
    cuantos += primo;
  }
  return cuantos;
}

void reportar(const char *nombre, long resultado, double t0) {
  printf("  %-8s hilo %d  %6.1f ms  resultado %ld\n", nombre,
         omp_get_thread_num(), (omp_get_wtime() - t0) * 1000, resultado);
}

int main() {
  setlocale(LC_NUMERIC, "");
  vector<int> a(4000000), b(60000000), c(1000000);
  for (size_t i = 0; i < a.size(); i++) a[i] = (int)(i * 2654435761u % 1000000);
  for (size_t i = 0; i < b.size(); i++) b[i] = (int)(i % 1000);
  for (size_t i = 0; i < c.size(); i++) c[i] = (int)(i * 7919 % 1000000);

  printf("uno tras otro\n");
  double t0 = omp_get_wtime(), t;
  t = omp_get_wtime(); reportar("ordenar", ordenar(a), t);
  t = omp_get_wtime(); reportar("sumar", sumar(b), t);
  t = omp_get_wtime(); reportar("primos", primos_en(c), t);
  printf("total %.1f ms\n", (omp_get_wtime() - t0) * 1000);

  printf("en secciones\n");
  t0 = omp_get_wtime();
#pragma omp parallel sections
  {
#pragma omp section
    { double t = omp_get_wtime(); reportar("ordenar", ordenar(a), t); }
#pragma omp section
    { double t = omp_get_wtime(); reportar("sumar", sumar(b), t); }
#pragma omp section
    { double t = omp_get_wtime(); reportar("primos", primos_en(c), t); }
  }
  printf("total %.1f ms\n", (omp_get_wtime() - t0) * 1000);
  return 0;
}
```

```bash
g++ -std=c++17 -O2 -fopenmp -o trabajos trabajos.cpp
./trabajos
```

```
uno tras otro
  ordenar  hilo 0   253,3 ms  resultado 500000
  sumar    hilo 0    26,2 ms  resultado 29970000000
  primos   hilo 0   344,9 ms  resultado 78498
total 624,8 ms
en secciones
  sumar    hilo 6    27,6 ms  resultado 29970000000
  ordenar  hilo 0   254,0 ms  resultado 500000
  primos   hilo 8   344,5 ms  resultado 78498
total 345,7 ms
```

Uno tras otro, el total es la suma de los tres: 624,8 ms. En secciones cada
trabajo cayó en un hilo distinto, el 6, el 0 y el 8, los tres arrancaron a la
vez y el total es el del más largo, 345,7 ms contra 344,5 de los primos: la
suma y el orden terminaron mucho antes y esos hilos se quedaron esperando en
la barrera. Los otros nueve hilos de la máquina no hicieron nada. El tercer
vector contiene cada entero entre 0 y 999.999 una sola vez, así que el conteo
es el de los primos menores que un millón, 78.498. Con dos hilos:

```bash
OMP_NUM_THREADS=2 ./trabajos
```

```
uno tras otro
  ordenar  hilo 0   253,3 ms  resultado 500000
  sumar    hilo 0    26,0 ms  resultado 29970000000
  primos   hilo 0   344,6 ms  resultado 78498
total 624,2 ms
en secciones
  sumar    hilo 1    26,7 ms  resultado 29970000000
  ordenar  hilo 0   252,1 ms  resultado 500000
  primos   hilo 1   367,2 ms  resultado 78498
total 394,0 ms
```

El hilo 1 hizo la suma y después los primos, uno tras otro, y el hilo 0 el
orden: el total, 394,0 ms, es lo que tardó el hilo 1 con sus dos trabajos.
Con dos hilos y tres trabajos a uno le tocan dos, y cuáles le tocan lo decide
el runtime, no el programa; aquí juntó la suma con los primos y el total quedó
50 ms por encima del trabajo más largo. Desde tres hilos en adelante las
secciones se quedan en los 345 ms de ese trabajo, tenga la máquina los
núcleos que tenga. Cuando los
trabajos recorren los mismos datos hay otra forma de repartir: un solo bucle
que haga todas las cuentas por elemento y reparta los datos entre todos los
hilos, con una reducción por resultado. Varias reducciones caben en una misma
directiva, y con `min` y `max` se ve además que el valor inicial de la
variable participa en la combinación.

`rango.cpp`:

```cpp
// El menor y el mayor de un vector en una sola pasada, con dos reducciones en
// la misma directiva. El valor que trae la variable de afuera también entra en
// la combinación: se ve con un máximo que arranca en 0 sobre valores negativos.
#include <climits>
#include <cstdio>
#include <vector>

int main() {
  std::vector<int> v(30000000);
  for (size_t i = 0; i < v.size(); i++) v[i] = -1000 + (int)(i * 7919 % 999);

  int menor = INT_MAX, mayor = INT_MIN, desde_cero = 0;
#pragma omp parallel for reduction(min : menor) reduction(max : mayor, desde_cero)
  for (size_t i = 0; i < v.size(); i++) {
    if (v[i] < menor) menor = v[i];
    if (v[i] > mayor) mayor = v[i];
    if (v[i] > desde_cero) desde_cero = v[i];
  }
  printf("menor %d mayor %d mayor arrancando en 0: %d\n", menor, mayor, desde_cero);
  return 0;
}
```

```bash
g++ -std=c++17 -O2 -fopenmp -o rango rango.cpp
OMP_NUM_THREADS=4 ./rango
```

```
menor -1000 mayor -2 mayor arrancando en 0: 0
```

Los valores van de −1.000 a −2. `mayor` arranca en `INT_MIN` y da −2;
`desde_cero` hace la misma cuenta y arranca en 0, y como 0 es mayor que todos
los valores, la combinación final lo devuelve. Un máximo se inicializa en
`INT_MIN` y un mínimo en `INT_MAX`, o en el primer elemento, nunca en 0.

### Lo que suele fallar

- **Las secciones tardan igual que la secuencial y todas dicen `hilo 0`.**
  Se escribió `#pragma omp sections` sin `parallel` y afuera de toda región:
  compila, corre y no reparte nada. En `trabajos.cpp` con esa directiva a
  secas, los tres trabajos dijeron `hilo 0` y el total fue 574,9 ms, la suma
  de los tres, contra 345,7 con `parallel sections`.
- **`error: expected ')' before '.' token`** en el `reduction`. La cláusula
  no acepta un miembro de una estructura como `r.suma`; se reduce sobre
  variables locales sueltas y al final se arman en la estructura.
- **Los resultados de la pasada única salen mal y cambian entre corridas.**
  Los tres acumuladores se escriben directamente desde el bucle sin ponerlos
  en `reduction`: es la carrera de la parte anterior, tres veces.
- **El máximo sale en 0 cuando todos los valores son negativos.** El valor
  inicial de la variable participa: `reduction(max : m)` con `m = 0` y todos
  los valores negativos devuelve 0. Por eso el `Resumen` del repositorio
  arranca en `INT_MIN`. `rango.cpp` lo muestra con las dos variables lado a
  lado: la que arranca en 0 devuelve 0 y la que arranca en `INT_MIN`, −2.
- **Las tres secciones no bajan a un tercio.** Cada sección sigue siendo un
  recorrido secuencial, el total lo marca la más larga, y los tres recorridos
  del mismo vector compiten además por el ancho de banda de memoria; un hilo
  sin nada que hacer no ayuda. La aceleración de `sections` está acotada por
  el número de trabajos y por el más largo de ellos, no por los núcleos.

### Enlaces

- [sections Construct, OpenMP 5.1](https://www.openmp.org/spec-html/5.1/openmpsu42.html): cómo se reparten las secciones entre los hilos y dónde está la barrera.
- [parallel Construct, OpenMP 5.1](https://www.openmp.org/spec-html/5.1/openmpse14.html): la región que `parallel sections` abre y cierra.
- [Reduction Clauses and Directives, OpenMP 5.1](https://www.openmp.org/spec-html/5.1/openmpsu117.html): los operadores `max` y `min`, su valor inicial y cómo se combinan varias cláusulas.
- [omp_get_num_threads, OpenMP 5.1](https://www.openmp.org/spec-html/5.1/openmpsu121.html): cuántos hilos hay en el equipo, para ver cuántos sobran con tres secciones.
- [OMP_NUM_THREADS, libgomp](https://gcc.gnu.org/onlinedocs/libgomp/OMP_005fNUM_005fTHREADS.html): la variable con la que se prueba el techo de las secciones sin recompilar.
- [Numeric limits, cppreference](https://en.cppreference.com/w/cpp/types/climits): `INT_MIN` e `INT_MAX`, los valores iniciales de un máximo y un mínimo.

## Cómo compilar y ejecutar en la máquina propia

### Debian y Ubuntu

OpenMP viene con `g++`: la biblioteca `libgomp` se instala junto al
compilador y no hay paquete aparte.

```bash
sudo apt update
sudo apt install -y build-essential
g++ --version
nproc          # cuántos procesadores ve el sistema
```

Las banderas son las del `Makefile`: `-std=c++17` por el lenguaje, `-O2`
para que el compilador optimice, porque sin optimizar los tiempos no dicen
nada, y `-fopenmp` para que las directivas cuenten y se enlace `libgomp`.

```bash
g++ -std=c++17 -O2 -fopenmp -o programa programa.cpp
OMP_NUM_THREADS=4 ./programa
OMP_DISPLAY_ENV=true ./programa    # la configuración de OpenMP al arrancar
```

`OMP_DISPLAY_ENV=true` imprime, antes de la primera línea del programa, la
versión de OpenMP y el valor de `OMP_NUM_THREADS`, `OMP_SCHEDULE` y las demás
variables: es la forma de saber qué está viendo el runtime.

### Windows con WSL2

Dentro de WSL2 se instala Ubuntu y se siguen las mismas instrucciones. Dos
cosas cambian. Los procesadores que ve `nproc` son los que Windows le asigna
a la máquina virtual, que pueden ser menos que los físicos, y se ajustan con
`processors=` en el archivo `.wslconfig` del usuario de Windows. Y el
repositorio se clona en el sistema de archivos de Linux, bajo `~/`, no en
`/mnt/c/`: compilar y escribir los `.txt` sobre `/mnt/c/` pasa por la capa
de traducción de archivos y es mucho más lento.

### macOS

El `g++` de macOS es un alias de Apple clang y no acepta `-fopenmp`. Hay dos
caminos.

El primero es el GCC de Homebrew. `brew install gcc` deja el compilador con
el número de versión en el nombre, `g++-16` al momento de escribir esto
(`ls "$(brew --prefix)/bin/"g++-*` lo dice). Como el `Makefile` usa la
variable `CXX`, se reemplaza desde la línea de comandos:

```bash
brew install gcc
make CXX=g++-16 hadamard
```

El segundo es Apple clang con la biblioteca `libomp` de LLVM. `brew install
libomp` la instala fuera del camino de búsqueda, así que las rutas van
explícitas al compilar a mano:

```bash
brew install libomp
clang++ -std=c++17 -O2 -Xpreprocessor -fopenmp \
  -I"$(brew --prefix libomp)/include" -L"$(brew --prefix libomp)/lib" -lomp \
  -o programa programa.cpp
```

Con cualquiera de los dos, `OMP_NUM_THREADS` y las funciones de `omp.h`
funcionan igual. `nproc` no existe en macOS; el equivalente es
`sysctl -n hw.ncpu`.

### Enlaces

- [GNU Offloading and Multi-Processing Runtime Library, libgomp](https://gcc.gnu.org/onlinedocs/libgomp/): el manual del runtime de GCC: variables de ambiente, funciones y detalles de implementación.
- [Options That Control Optimization, GCC](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html): qué activa `-O2` y qué diferencia hay con `-O0` y `-O3`.
- [make, POSIX (The Open Group)](https://pubs.opengroup.org/onlinepubs/9699919799/utilities/make.html): la especificación de `make`, con el operando `macro=value` que hace que `make CXX=g++-16` reemplace la variable del `Makefile`.
- [Install WSL, Microsoft Learn](https://learn.microsoft.com/en-us/windows/wsl/install): la instalación de WSL2 y de Ubuntu adentro.
- [Advanced settings configuration in WSL, Microsoft Learn](https://learn.microsoft.com/en-us/windows/wsl/wsl-config): el archivo `.wslconfig`, con `processors=` y `memory=`.
- [gcc, Homebrew](https://formulae.brew.sh/formula/gcc) y [libomp, Homebrew](https://formulae.brew.sh/formula/libomp): los dos paquetes de macOS, con su versión actual.
- [OpenMP Support, Clang](https://clang.llvm.org/docs/OpenMPSupport.html): qué partes de OpenMP implementa clang.
- [OMP_DISPLAY_ENV, OpenMP 5.1](https://www.openmp.org/spec-html/5.1/openmpse69.html): la variable que imprime la configuración del runtime al arrancar.
