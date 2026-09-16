// Un histograma de veinte millones de valores en dieciséis cajones, con varios
// hilos escribiendo sobre los mismos contadores: la carrera de datos, y tres
// formas de evitarla.
#include <chrono>
#include <cstdio>
#include <omp.h>
#include <vector>

using namespace std;
using namespace std::chrono;

const size_t N = 20000000;
const int CAJONES = 16;

// Llenado determinista: la misma secuencia en todas las máquinas.
void llenar(vector<unsigned char> &v) {
  unsigned long long s = 20261006;
  for (size_t i = 0; i < v.size(); i++) {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    v[i] = (unsigned char)((s >> 60) & 15);  // un cajón entre 0 y 15
  }
}

// Un solo hilo. Es el punto de comparación.
vector<long> secuencial(const vector<unsigned char> &v) {
  vector<long> cuentas(CAJONES, 0);
  for (size_t i = 0; i < v.size(); i++) cuentas[v[i]]++;
  return cuentas;
}

// Varios hilos incrementan los mismos contadores sin protección. Está escrito
// así a propósito: es la versión que cuenta mal.
vector<long> con_carrera(const vector<unsigned char> &v) {
  vector<long> cuentas(CAJONES, 0);
#pragma omp parallel for
  for (size_t i = 0; i < v.size(); i++) cuentas[v[i]]++;
  return cuentas;
}

// TODO: cada incremento es una operación atómica.
vector<long> con_atomic(const vector<unsigned char> &v) {
  vector<long> cuentas(CAJONES, 0);
  return cuentas;
}

// TODO: cada incremento entra a una sección crítica.
vector<long> con_critical(const vector<unsigned char> &v) {
  vector<long> cuentas(CAJONES, 0);
  return cuentas;
}

// TODO: cada hilo cuenta en su copia privada y las copias se suman al final.
// OpenMP reduce arreglos con reduction(+ : cuentas[:CAJONES]) sobre un
// arreglo de C; también sirve una copia privada por hilo sumada a mano en
// una sección crítica al terminar.
vector<long> con_reduccion(const vector<unsigned char> &v) {
  vector<long> cuentas(CAJONES, 0);
  return cuentas;
}

long total(const vector<long> &c) {
  long t = 0;
  for (long x : c) t += x;
  return t;
}

int main() {
  vector<unsigned char> v(N);
  llenar(v);
  vector<long> base = secuencial(v);

  struct { const char *nombre; vector<long> (*f)(const vector<unsigned char> &); } versiones[] = {
      {"secuencial", secuencial}, {"carrera", con_carrera}, {"atomic", con_atomic},
      {"critical", con_critical}, {"reduccion", con_reduccion}};

  for (auto &ver : versiones) {
    auto t0 = high_resolution_clock::now();
    vector<long> c = ver.f(v);
    auto t1 = high_resolution_clock::now();
    printf("%s %.1f ms total %ld cajon0 %ld %s\n", ver.nombre,
           duration_cast<microseconds>(t1 - t0).count() / 1000.0, total(c), c[0],
           c == base ? "coincide" : "DIFIERE");
  }
  return 0;
}
