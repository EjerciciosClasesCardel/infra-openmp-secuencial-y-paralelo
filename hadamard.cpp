// Producto de Hadamard con OpenMP: w[i] = u[i] * v[i] y la suma de w, en
// secuencial y con parallel for. El programa recorre 1, 2, 4 y 8 hilos e
// imprime la aceleración y la eficiencia de cada corrida.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <omp.h>
#include <vector>

using namespace std;
using namespace std::chrono;

const int A = 4;  // valor constante de u
const int B = 9;  // valor constante de v

// Un solo hilo, sin directivas. Es el punto de comparación.
long secuencial(const vector<int> &u, const vector<int> &v, vector<int> &w) {
  size_t n = u.size();
  for (size_t i = 0; i < n; i++) w[i] = u[i] * v[i];
  long suma = 0;
  for (size_t i = 0; i < n; i++) suma += w[i];
  return suma;
}

// TODO: el producto con parallel for y la suma con reduction. Todos los
// hilos acumulan sobre la misma variable, y sin la cláusula el resultado
// sale mal.
long paralelo(const vector<int> &u, const vector<int> &v, vector<int> &w) {
  return 0;
}

int main(int argc, char **argv) {
  size_t n = argc > 1 ? strtoull(argv[1], nullptr, 10) : 100000000;
  vector<int> u(n, A), v(n, B), w(n, 0);

  auto t0 = high_resolution_clock::now();
  long base = secuencial(u, v, w);
  auto t1 = high_resolution_clock::now();
  double ms_base = duration_cast<microseconds>(t1 - t0).count() / 1000.0;
  printf("secuencial n %zu %.1f ms suma %ld\n", n, ms_base, base);

  for (int k : {1, 2, 4, 8}) {
    omp_set_num_threads(k);
    auto a = high_resolution_clock::now();
    long suma = paralelo(u, v, w);
    auto b = high_resolution_clock::now();
    double ms = duration_cast<microseconds>(b - a).count() / 1000.0;
    printf("hilos %d n %zu %.1f ms suma %ld aceleracion %.2f eficiencia %.2f\n",
           k, n, ms, suma, ms_base / ms, ms_base / ms / k);
  }
  return 0;
}
