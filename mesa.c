#include "mesa.h"
#include <stdio.h>
#include <stdlib.h>

//funcionamiento de la partida, mutex, tablero, movimientos

void agregar_vecino(struct Casilla *c, int id_vecino) {// funcion auxiliar para insertar las conexiones de manera limpia
    if (c->cant_vecinos < 8) {
        c->id_vecinos[c->cant_vecinos] = id_vecino;
        c->cant_vecinos++;
    }
}

void mapear_vecinos(struct Mesa *mesa) {
    // conexiones para la parte de 5*5 de la cuadricula (va desde el indice 3 al 27)
    for (int fila = 0; fila < 5; fila++) {
        for (int col = 0; col < 5; col++) {
            int actual = 3 + (fila * 5) + col;

            // Conexiones ortogonales
            if (col > 0) agregar_vecino(&mesa->tablero[actual], actual - 1); // izquierda
            if (col < 4) agregar_vecino(&mesa->tablero[actual], actual + 1); //derecha
            if (fila > 0) agregar_vecino(&mesa->tablero[actual], actual - 5); // arriba
            if (fila < 4) agregar_vecino(&mesa->tablero[actual], actual + 5); // abajo

            // conexiones diagonales (solo en intersecciones donde fila+col es par)
            if ((fila + col) % 2 == 0) {
                if (fila > 0 && col > 0) agregar_vecino(&mesa->tablero[actual], actual - 6);
                if (fila > 0 && col < 4) agregar_vecino(&mesa->tablero[actual], actual - 4);
                if (fila < 4 && col > 0) agregar_vecino(&mesa->tablero[actual], actual + 4);
                if (fila < 4 && col < 4) agregar_vecino(&mesa->tablero[actual], actual + 6);
            }
        }
    }

    // conexiones manuales en la parte asimetrica (indices 0, 1 y 2)
    // nodo 0: trono
    agregar_vecino(&mesa->tablero[0], 1);
    agregar_vecino(&mesa->tablero[0], 2);
    agregar_vecino(&mesa->tablero[0], 5); // Conecta directo al centro de la primera fila

    // nodo 1: abajo del trono lado izq
    agregar_vecino(&mesa->tablero[1], 0);
    agregar_vecino(&mesa->tablero[1], 2);
    agregar_vecino(&mesa->tablero[1], 3); // esquina sup izq de la cuadricula
    agregar_vecino(&mesa->tablero[1], 5); // centro sup de la cuadricula

    // nodo 2: abajo del trono lado der
    agregar_vecino(&mesa->tablero[2], 0);
    agregar_vecino(&mesa->tablero[2], 1);
    agregar_vecino(&mesa->tablero[2], 5);
    agregar_vecino(&mesa->tablero[2], 7); // Esquina sup der de la cuadricula

    // conexiones de retorno desde la base de la cuadricula hacia el trono
    agregar_vecino(&mesa->tablero[3], 1);
    agregar_vecino(&mesa->tablero[5], 0);
    agregar_vecino(&mesa->tablero[5], 1);
    agregar_vecino(&mesa->tablero[5], 2);
    agregar_vecino(&mesa->tablero[7], 2);
}

void inicializar_mesa(struct Mesa *mesa) {
    mesa->juego_activo = true;
    mesa->perros_capturados = 0;
    mesa->estado = 1; //asi marcamos la mesa como ocupada

    // se nicializan todas las casillas
    for (int i = 0; i < num_casillas; i++) {
        mesa->tablero[i].nro_casilla = i;
        mesa->tablero[i].ocupante = libre;
        mesa->tablero[i].cant_vecinos = 0;
        
        if (pthread_mutex_init(&mesa->tablero[i].lock, NULL) != 0) {
            perror("error al inicializar el mutex de la casilla");
            exit(EXIT_FAILURE);
        }
    }

    // aqui llamaremos a la funcion que conecta los vecinos
    mapear_vecinos(mesa);

    // Aqu pondremos las fichas en sus posiciones iniciales
    mesa->tablero[0].ocupante = puma;
    
    // asignar los 12 perros en las filas inferiores (como por ej los indices 16 al 27)
    for (int i = 16; i < num_casillas; i++) {
        mesa->tablero[i].ocupante = perro;
    }
}

