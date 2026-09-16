CXX = g++
FLAGS = -std=c++17 -O2 -fopenmp

# Parte 1: producto de Hadamard, secuencial y con parallel for.
hadamard:
	$(CXX) $(FLAGS) -o hadamard hadamard.cpp
	./hadamard | tee hadamard.txt
	rm -f hadamard

# Parte 2: schedule sobre tareas de costo desigual.
reparto:
	$(CXX) $(FLAGS) -o reparto reparto.cpp
	./reparto | tee reparto.txt
	rm -f reparto

# Parte 3: la carrera de datos y tres formas de evitarla.
histograma:
	$(CXX) $(FLAGS) -o histograma histograma.cpp
	OMP_NUM_THREADS=4 ./histograma | tee histograma.txt
	rm -f histograma

# Parte 4: descomposición de tareas con sections.
secciones:
	$(CXX) $(FLAGS) -o secciones secciones.cpp
	./secciones | tee secciones.txt
	rm -f secciones

todo: hadamard reparto histograma secciones

limpiar:
	rm -f hadamard reparto histograma secciones
	rm -f hadamard.txt reparto.txt histograma.txt secciones.txt
