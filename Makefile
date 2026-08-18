secuencial:
	g++ -o exe secuencial.cpp 
	./exe > secuencial.txt
	rm exe

paralelo:
	g++ -o exe paralelo.cpp -O3 -ffast-math -fopenmp
	./exe > paralelo.txt
	rm exe
