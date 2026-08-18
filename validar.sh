#!/bin/bash

verificar_archivo() {
	local nombre_archivo=$1

	if [ ! -f "$nombre_archivo" ]; then
		echo "Error: El archivo '$nombre_archivo' no existe."
		return 1
	fi

	local cantidad_caracteres=$(wc -m <"$nombre_archivo" | tr -d ' ')

	if [ "$cantidad_caracteres" -ge 10 ]; then
		return 0
	else
		return 1
	fi
}

verificar_archivo "$1"
