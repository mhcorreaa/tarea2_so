#ifndef MESA_H
#define MESA_H

#define puma 0
#define perro 1
#define libre 2
#define num_casillas 28 // lo cambie martin para mejor manejo en los for

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
    struct Casilla tablero[num_casillas]; 

    // variables para el control del juez sobre la partida(si la termina ono)
    bool juego_activo;
    int perros_capturados;
};

// estructura para pasar argumentos a los hilos
struct infojugador {
    int id_jugador;
    int tipo_ficha; // puma o perro
    struct Mesa *mesa;
};

#endif
