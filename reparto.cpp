// Planificación con schedule: sesenta y cuatro tareas de costo desigual
// repartidas entre cuatro hilos de tres maneras, sobre dos patrones de costo.
#include <chrono>
#include <cstdio>
#include <omp.h>
#include <vector>

using namespace std;
using namespace std::chrono;

const int TAREAS = 64;
const int HILOS = 4;

// Trabajo sintético proporcional a `costo`.
long trabajo(long costo) {
  long s = 0;
  for (long k = 0; k < costo; k++) s += k % 3;
  return s;
}

// Patrón creciente: la tarea i cuesta proporcional a la cuarta potencia de i.
long creciente(int i) { return 8L * i * i * i * i; }

// Patrón periódico: una de cada cuatro tareas cuesta veinticinco veces las otras.
long periodico(int i) { return (i % 4 == 3) ? 25 * 3800000L : 3800000L; }

using Patron = long (*)(int);

// Bloques contiguos: schedule(static) reparte las iteraciones en bloques
// del mismo tamaño, uno por hilo.
long bloques(Patron costo) {
  long total = 0;
#pragma omp parallel for schedule(static) reduction(+ : total)
  for (int i = 0; i < TAREAS; i++) total += trabajo(costo(i));
  return total;
}

// TODO: por turnos. El hilo h hace las iteraciones h, h + 4, h + 8, ...
// Es una cláusula schedule con un tamaño de bloque.
long turnos(Patron costo) {
  return 0;
}

// TODO: por demanda. La siguiente iteración libre se la lleva el hilo que se
// desocupa. También es una cláusula schedule.
long demanda(Patron costo) {
  return 0;
}

int main() {
  omp_set_num_threads(HILOS);
  struct { const char *nombre; Patron costo; } patrones[] = {
      {"creciente", creciente}, {"periodico", periodico}};
  struct { const char *nombre; long (*reparto)(Patron); } repartos[] = {
      {"bloques", bloques}, {"turnos", turnos}, {"demanda", demanda}};

  for (auto &p : patrones)
    for (auto &r : repartos) {
      auto t0 = high_resolution_clock::now();
      long total = r.reparto(p.costo);
      auto t1 = high_resolution_clock::now();
      printf("%s %s %.1f ms total %ld\n", p.nombre, r.nombre,
             duration_cast<microseconds>(t1 - t0).count() / 1000.0, total);
    }
  return 0;
}
