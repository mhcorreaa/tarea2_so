#include "mesa.h"
#include <signal.h> // ctrl + c
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/wait.h>

struct Server{
    int cant_players;
    int cant_mesas;
    int min_time;
    int max_time;
    pid_t pid_mesas[];
};

// funcion matematica para calcular donde aterriza el Puma al saltar para comer un perro primero recibe la casilla de origen y la casilla del medio (donde esta el perro) y devuelve la casilla destino donde aterriza el puma si no se puede saltar devuelve -1
int calcular_salto(int origen, int medio) {
    // casos en los que el salto no se puede realizar por estar en los bordes del tablero
    if (origen == 0 && medio == 1) return 3;
    if (origen == 3 && medio == 1) return 0;
    if (origen == 0 && medio == 2) return 7;
    if (origen == 7 && medio == 2) return 0;
    if (origen == 0 && medio == 5) return 10;
    if (origen == 10 && medio == 5) return 0;

    // calculo de linea recta para la parte del tablero que es 5x5
    int delta = medio - origen;
    int destino = medio + delta;

    // validar que no se salga del tablero ni haga lo del pac man en los bordes
    if (destino < 3 || destino >= num_casillas) return -1;
    
    int col_origen = (origen - 3) % 5;
    int col_destino = (destino - 3) % 5;
    if (abs(col_origen - col_destino) > 2) return -1; // si la diferencia de columnas es mayor a 2 significa que se salto de un borde al otro lo cual no es permitido

    return destino;
}

// rutina que ejecutan de manera concurrente los hilos de los jugadores (puma y perros) para moverse por el tablero y capturar o ser capturados
void *rutina_jugador(void *arg) {
    struct infojugador *info = (struct infojugador *)arg;
    struct Mesa *mesa = info->mesa;
    
    unsigned int seed = time(NULL) + info->id_jugador;

    while (mesa->juego_activo) {
        // retardo aleatorio entre 50 y 300 ms que pedia el enunciado para simular el tiempo de decision de cada jugador
        int ms_retardo = (rand_r(&seed) % 251) + 50;
        usleep(ms_retardo * 1000);

        if (!mesa->juego_activo) break;

        int pos_actual = info->pos_actual;
        if (mesa->tablero[pos_actual].cant_vecinos == 0) continue;
        
        int indice_vecino = rand_r(&seed) % mesa->tablero[pos_actual].cant_vecinos;
        int vecino = mesa->tablero[pos_actual].id_vecinos[indice_vecino];

        // salto del puma para capturar al perro si es posible
        int destino_salto = -1;
        if (info->tipo_ficha == puma && mesa->tablero[vecino].ocupante == perro) {
            destino_salto = calcular_salto(pos_actual, vecino);
        }

        if (destino_salto != -1) {
            // regla anti-deadlock para 3 casillas ordenar IDs de menor a mayor como se deidio para evitar que los hilos se bloqueen entre si
            int ids[3] = {pos_actual, vecino, destino_salto};
            for (int x = 0; x < 2; x++) {
                for (int y = 0; y < 2 - x; y++) {
                    if (ids[y] > ids[y+1]) {
                        int temp = ids[y];
                        ids[y] = ids[y+1];
                        ids[y+1] = temp;
                    }
                }
            }

            // bloquear estrictamente en orden
            pthread_mutex_lock(&mesa->tablero[ids[0]].lock);
            pthread_mutex_lock(&mesa->tablero[ids[1]].lock);
            pthread_mutex_lock(&mesa->tablero[ids[2]].lock);

            // validar que el puma sigue en la casilla de origen y que el perro sigue en la casilla del medio y la casilla destino esta libre antes de efectuar el salto
            if (mesa->tablero[pos_actual].ocupante == puma && 
                mesa->tablero[vecino].ocupante == perro && 
                mesa->tablero[destino_salto].ocupante == libre) {
                
                // se realiza el salto del puma por lo que se come al perro y actualiza las posiciones de las casillas y del jugador
                mesa->tablero[vecino].ocupante = libre; 
                mesa->tablero[destino_salto].ocupante = puma;
                mesa->tablero[pos_actual].ocupante = libre;
                info->pos_actual = destino_salto;
                
                mesa->perros_capturados++; // sumamos punto al juez
                printf("\n el puma salto y se comio al perro %d  cayendo en %d, total capturados: %d\n\n", vecino, destino_salto, mesa->perros_capturados);
            }

            // liberar en orden inverso
            pthread_mutex_unlock(&mesa->tablero[ids[2]].lock);
            pthread_mutex_unlock(&mesa->tablero[ids[1]].lock);
            pthread_mutex_unlock(&mesa->tablero[ids[0]].lock);
        } 
        // movimiento normal de puma o perro a una casilla vecina libre
        else {
            int id_primero = (pos_actual < vecino) ? pos_actual : vecino;
            int id_segundo = (pos_actual < vecino) ? vecino : pos_actual;

            pthread_mutex_lock(&mesa->tablero[id_primero].lock);
            pthread_mutex_lock(&mesa->tablero[id_segundo].lock);

            if (mesa->tablero[vecino].ocupante == libre) {
                mesa->tablero[vecino].ocupante = info->tipo_ficha;
                mesa->tablero[pos_actual].ocupante = libre;
                info->pos_actual = vecino;
                
                // Printf condicional para encontrar donde se pego el programa en caso de un deadlock
                // printf("[ficha %d] se movio de %d a %d\n", info->id_jugador, pos_actual, vecino);
            }

            pthread_mutex_unlock(&mesa->tablero[id_segundo].lock);
            pthread_mutex_unlock(&mesa->tablero[id_primero].lock);
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    // lectura y validacion de para los n jugadores y m mesas
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <N_jugadores> <M_mesas>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int n_jugadores = atoi(argv[1]);
    int m_mesas = atoi(argv[2]);

    if (n_jugadores < 13 || m_mesas < 1) {
        fprintf(stderr, "error: deben haber minimo 13 jugadores (1 puma, 12 perros) y 1 mesa.\n");
        exit(EXIT_FAILURE);
    }

    printf("iniciando servidor: %d jugadores distribuidos en %d mesas \n", n_jugadores, m_mesas);

    // arreglo dinamico para que se pueda guardar los PIDs de las mesas
    pid_t *pid_mesas = malloc(m_mesas * sizeof(pid_t));
    if (pid_mesas == NULL) {
        perror("Error de memoria");
        exit(EXIT_FAILURE);
    }

    // aqui se crean los procesos hijos aislados mediante fork()
    for (int i = 0; i < m_mesas; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("error al hacer fork ya que no se pudo crear el proceso hijo");
            exit(EXIT_FAILURE);
        } 
        else if (pid == 0) {
            printf("[mesa %d] iniciada con PID: %d\n", i + 1, getpid());
            
            struct Mesa mesa;
            inicializar_mesa(&mesa); // prepara los mutex y las conexiones

            pthread_t hilos[13];
            struct infojugador info_hilos[13];

            // se crea el hilo del Puma (ID 0)
            info_hilos[0].id_jugador = 0;
            info_hilos[0].tipo_ficha = puma;
            info_hilos[0].pos_actual = 0; // parte en el trono (casilla 0)
            info_hilos[0].mesa = &mesa;
            pthread_create(&hilos[0], NULL, rutina_jugador, (void *)&info_hilos[0]);

            // se crean los 12 hilos de los Perros (IDs 1 al 12)
            for (int j = 1; j <= 12; j++) {
                info_hilos[j].id_jugador = j;
                info_hilos[j].tipo_ficha = perro;
                info_hilos[j].pos_actual = 15 + j; // parten en las casillas 16 a 27
                info_hilos[j].mesa = &mesa;
                pthread_create(&hilos[j], NULL, rutina_jugador, (void *)&info_hilos[j]);
            }

           // logica del juez para que monitoree el juego y determine si hay un ganador o no se va a ejecutar mientras el juego este activo
            while (mesa.juego_activo) {
                usleep(500000); // el juez va a revisar el tablero cada 500 ms

                // primera condicion de victoria cuando el puma captura 5 perros se termina el juego y se declara ganador al puma
                if (mesa.perros_capturados >= 5) {
                    printf("\n[juez mesa %d] gana el PUMA ya que capturo 5 perros.\n", i + 1);
                    mesa.juego_activo = false;
                    break;
                }

                // segunda condicion de victoria cuando los perros acorralan al puma y este ya no tiene casillas donde moverse se termina el juego y se declara ganador a los perros
                int pos_puma = info_hilos[0].pos_actual;
                bool puma_atrapado = true;

                // bloqueamos la casilla del puma para leer sus opciones
                pthread_mutex_lock(&mesa.tablero[pos_puma].lock);
                
                for (int v = 0; v < mesa.tablero[pos_puma].cant_vecinos; v++) {
                    int destino = mesa.tablero[pos_puma].id_vecinos[v];
                    // lectura para revisar si hay al menos una casilla libre
                    if (mesa.tablero[destino].ocupante == libre) {
                        puma_atrapado = false;
                        break;
                    }
                }
                
                pthread_mutex_unlock(&mesa.tablero[pos_puma].lock);

                if (puma_atrapado) {
                    printf("\n[juez mesa %d] ganan los perros ya que el puma no tiene donde moverse, esta acorralado\n", i + 1);
                    mesa.juego_activo = false;
                    break;
                }
            }

            // el juez espera a que los hilos terminen antes de finalizar el proceso hijo de la mesa
            for (int j = 0; j < 13; j++) {
                pthread_join(hilos[j], NULL);
            } // hasta aca llega la logica del juez

            exit(EXIT_SUCCESS); // el hijo muere aqui cuando termina su partida
        }
        else {
            pid_mesas[i] = pid; // guardamos el PID del hijo
        }
    }

    // el padre espera a que todas las mesas terminen de jugar antes de finalizar el torneo y cerrar el servidor
    for (int i = 0; i < m_mesas; i++) {
        waitpid(pid_mesas[i], NULL, 0);
    }

    free(pid_mesas);
    printf("torneo terminado ya todas las mesas se cerraron correctamente \n");
    return 0;
}