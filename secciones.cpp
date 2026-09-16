// Descomposición de tareas con OpenMP: tres recorridos distintos del mismo
// vector, cada uno en una sección.
#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdio>
#include <omp.h>
#include <vector>

using namespace std;
using namespace std::chrono;

const size_t N = 100000000;

struct Resumen {
  long suma;
  int maximo;
  long mayores;  // cuántos elementos superan el umbral
};

const int UMBRAL = 1000000000;

// Llenado determinista: la misma secuencia en todas las máquinas.
void llenar(vector<int> &v) {
  unsigned long long s = 20261006;
  for (size_t i = 0; i < v.size(); i++) {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    v[i] = (int)(s >> 33);
  }
}

// Los tres recorridos, uno tras otro.
Resumen secuencial(const vector<int> &v) {
  Resumen r{0, INT_MIN, 0};
  for (size_t i = 0; i < v.size(); i++) r.suma += v[i];
  for (size_t i = 0; i < v.size(); i++) r.maximo = max(r.maximo, v[i]);
  for (size_t i = 0; i < v.size(); i++) if (v[i] > UMBRAL) r.mayores++;
  return r;
}

// TODO: los tres recorridos a la vez, cada uno en una sección de un
// parallel sections. Adentro de una sección corre un solo hilo, así que cada
// recorrido sigue siendo secuencial; lo que se reparte son los tres trabajos.
Resumen con_secciones(const vector<int> &v) {
  Resumen r{0, INT_MIN, 0};
  return r;
}

// TODO: los tres resultados en una sola pasada repartida por datos: un
// parallel for con tres reducciones, una por resultado. La del máximo usa el
// operador max.
Resumen en_una_pasada(const vector<int> &v) {
  Resumen r{0, INT_MIN, 0};
  return r;
}

int main() {
  vector<int> v(N);
  llenar(v);

  struct { const char *nombre; Resumen (*f)(const vector<int> &); } versiones[] = {
      {"secuencial", secuencial}, {"secciones", con_secciones}, {"una_pasada", en_una_pasada}};

  for (auto &ver : versiones) {
    auto t0 = high_resolution_clock::now();
    Resumen r = ver.f(v);
    auto t1 = high_resolution_clock::now();
    printf("%s %.1f ms suma %ld maximo %d mayores %ld\n", ver.nombre,
           duration_cast<microseconds>(t1 - t0).count() / 1000.0, r.suma, r.maximo, r.mayores);
  }
  return 0;
}
