#include "mesa.h"
#include <signal.h> // ctrl + c
#include <unistd.h>

struct Server{
    
    int cant_players;
    int cant_mesas;
    int min_time;
    int max_time;
    pid_t pid_mesas[];
};

//funcion que mande los jugadores a alguna mesa para jugar

//funcion para el ctrl + c

int main(int argc, char *argv[]){


}