#ifndef MESA_H
#define MESA_H

#define puma 0
#define perro 1
#define libre 2
#define num_casillas 28 

#include <pthread.h>
#include <stdbool.h> 

struct Casilla{
    int nro_casilla;      
    int ocupante;  
    pthread_mutex_t lock;   
    
    int id_vecinos[8]; 
    int cant_vecinos;       
};

struct Mesa{
    int estado; //0 libre, 1 ocupada
    struct Casilla casilla[num_casillas]; 

    // variables para el control del juez sobre la partida
    bool juego_activo;
    int perros_capturados;
};

// estructura para pasar argumentos a los hilos
struct infojugador {
    int id_jugador;
    int tipo_ficha; // puma o perro
    int pos_actual;
    struct Mesa *mesa;
};

// invoca el servidor
void iniciarPartida(int id_mesa);

#endif