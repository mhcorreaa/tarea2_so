#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "mesa.h"
#include <signal.h> 
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <ctype.h>
#include <stdbool.h>

pid_t *pid_mesas = NULL;
int m_mesas = 0;

void detenerJuego(int sig){ //funcion ctrl + c
    (void)sig;
    printf("\nInterrumpiendo Servidor. Cerrando todas las mesas.\n");
    
    // enviar senal de aborto a los hijos
    for(int i = 0; i < m_mesas; i++){
        if(pid_mesas[i] > 0){
            kill(pid_mesas[i], SIGTERM);
        }
    }

    for(int i = 0; i < m_mesas; i++){
        if(pid_mesas[i] > 0){
            waitpid(pid_mesas[i], NULL, 0);
        }
    }

    free(pid_mesas);
    printf("CTRL + C. Partidas Terminadas.\n");
    exit(0);
}

// validar texto de entrada en invocacion
bool esNumero(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (!isdigit(str[i])) return false;
    }
    return true;
}

int main(int argc, char *argv[]){

    signal(SIGINT, detenerJuego); //ctrl + c

    if (argc != 3) {
        printf("Uso: %s <N_jugadores> <M_mesas>\n", argv[0]);
        return -1;
    }

    if (!esNumero(argv[1]) || !esNumero(argv[2])) {
        printf("ERROR: Los argumentos deben ser numeros enteros positivos.\n");
        return -1;
    }

    int n_jugadores = atoi(argv[1]);
    m_mesas = atoi(argv[2]);

    // grupos de 2 jugadores
    if (n_jugadores < 2 || n_jugadores % 2 != 0 || m_mesas < 1) {
        printf("ERROR: Deben haber minimo 2 jugadores\n");
        return -1;
    }

    int partidas_posibles = n_jugadores / 2;
    int mesas_por_crear;

    if (partidas_posibles < m_mesas) {
        mesas_por_crear = partidas_posibles;
    } else {
        mesas_por_crear = m_mesas;
    }

    printf("Iniciando servidor: %d jugadores formaran %d partidas concurrentes (Max mesas: %d)\n", n_jugadores, mesas_por_crear, m_mesas);

    pid_mesas = malloc(m_mesas * sizeof(pid_t));
    if (pid_mesas == NULL) {
        perror("Error de memoria");
        return -1;
    }

    // limpieza del arreglo
    for(int i = 0; i < m_mesas; i++) pid_mesas[i] = 0;

    for (int i = 0; i < mesas_por_crear; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("Error al hacer fork");
            return -1;
        } 
        else if (pid == 0) {
            printf("[Mesa %d] Iniciada con PID: %d\n", i + 1, getpid());
            iniciarPartida(i); // se ejecuta la mesa
        }
        else {
            pid_mesas[i] = pid; 
        }
    }

    // el padre espera a los hijos
    for (int i = 0; i < mesas_por_crear; i++) {
        waitpid(pid_mesas[i], NULL, 0);
    }

    free(pid_mesas);
    printf("Torneo terminado. Todas las mesas se cerraron correctamente.\n");
    return 0;
}