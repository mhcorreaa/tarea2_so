#ifndef MESA_H
#define MESA_H

#define puma 0
#define perro 1
#define libre 2
#define num_casillas 28 

#include <pthread.h>
#include <stdbool.h> 
#include <signal.h>

struct Config {
    int min_time;
    int max_time;
    unsigned int seed;
    int posiciones[13]; // 0 puma, resto perros
};

struct Casilla {
    int nro_casilla;      
    int ocupante;
    int id_ficha;  // id_jugador de la ficha que esta en la casilla, -1 si esta libre
    pthread_mutex_t lock;   
    
    int id_vecinos[8]; 
    int cant_vecinos;       
};

struct Mesa {
    int estado; //0 libre, 1 ocupada
    struct Casilla casilla[num_casillas]; 

    // variables para el control del juez sobre la partida
    volatile sig_atomic_t juego_activo;
    int perros_capturados;
    bool viva[13]; // false si la ficha fue capturada, su hilo termina

    pthread_mutex_t lock_juez;
    
    struct Config config; // se guarda la config
};

// estructura para pasar argumentos a los hilos
struct infojugador {
    int id_jugador;
    int tipo_ficha; // puma o perro
    int pos_actual;
    struct Mesa *mesa;
};

// invoca el servidor
void iniciarPartida(int id_mesa, struct Config *config_juego);

#endif