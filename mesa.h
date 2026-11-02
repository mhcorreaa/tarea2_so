#ifndef MESA_H
#define MESA_H

#define puma 0
#define perro 1
#define libre 2

#include <pthread.h> 

struct Casilla{
    int nro_casilla;      
    int ocupante;  
    pthread_mutex_t lock;   
    
    int id_vecinos[8]; 
    int cant_vecinos;       
};

struct Mesa{

    int estado; //0 libre, 1 ocupada
    struct Casilla tablero[28]; 
};

#endif