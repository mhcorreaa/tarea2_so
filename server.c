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
#include <string.h> 
#include <time.h>   

volatile sig_atomic_t server_activo = 1;
pid_t *pid_mesas = NULL;
int m_mesas = 0;

void detenerJuego(int sig){  //ctrl + c
    (void)sig;
    server_activo = 0; // martin aca cambie para arreglar lo que hablamos de printf o exit 
}

void corregirPosiciones(struct Config *config_juego) { //evita dos fichas en la misma casilla (por defecto o por config)

    bool usada[num_casillas] = {false};

    for (int i = 0; i < 13; i++) {

        int pos = config_juego->posiciones[i];

        if (pos < 0 || pos >= num_casillas || usada[pos]) { //casilla invalida o ya ocupada

            int casilla_libre = 0;
            while (usada[casilla_libre]) casilla_libre++; //primera casilla sin ficha

            printf("Advertencia: la ficha %d no puede partir en la casilla %d (ocupada o invalida), se mueve a la casilla %d.\n", i, pos, casilla_libre);
            config_juego->posiciones[i] = casilla_libre;
            pos = casilla_libre;
        }
        usada[pos] = true;
    }
}

bool esNumero(char *str) {  //revisa que los argumentos en la invocacion sean nros
    for (int i = 0; str[i] != '\0'; i++) {
        if (!isdigit(str[i])) return false;
    }
    return true;
}

void leerConfig(char *nombre_archivo, struct Config *config_juego) { //lee el archivo config
    
    FILE *read = fopen(nombre_archivo, "r");

    if (read == NULL){

        printf("Advertencia: No se encontro '%s', usando config predeterminada.\n", nombre_archivo);
        return;
    }

    char buffer[512];

    int temp_min_time = 0, temp_max_time = 0;

    while (fgets(buffer, sizeof(buffer), read) != NULL){

        char *linea = buffer;
        
        char *clave = strsep(&linea, ":");
        char *valor = strsep(&linea, "\n"); 
        
        if (valor != NULL) valor[strcspn(valor, "\r")] = '\0'; //por si config.txt lo crean en windows

        if (clave != NULL && valor != NULL && strlen(valor) > 0){

            if (strcmp(clave, "seed") == 0 && esNumero(valor)) config_juego->seed = (unsigned int)atoi(valor);
            
            else if (strcmp(clave, "min_time") == 0 && esNumero(valor)) temp_min_time = atoi(valor);
                
            else if (strcmp(clave, "max_time") == 0 && esNumero(valor)) temp_max_time = atoi(valor);
                
            else if (strcmp(clave, "positions") == 0) {
                
                char *pos_str = valor;

                for (int i = 0; i < 13; i++) {

                    char *num = strsep(&pos_str, ",");
                    
                    if (num != NULL && strlen(num) > 0 && esNumero(num) && atoi(num) < num_casillas) config_juego->posiciones[i] = atoi(num);
                }
            }
        }
    }

    fclose(read);

    if(temp_min_time <= temp_max_time){ //valida que el tiempo de espera sea valido antes de ingresarlo

        config_juego->min_time = temp_min_time;
        config_juego->max_time = temp_max_time;
    }

    printf("Configuracion cargada desde '%s'.\n", nombre_archivo);
}

int main(int argc, char *argv[]){

    struct sigaction sa;
    sa.sa_handler = detenerJuego;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // sin SA_RESTART para que interrumpa el wait
    sigaction(SIGINT, &sa, NULL);

    if (argc < 3 || argc > 4) {
        printf("Uso: %s <N_jugadores> <M_mesas> [config.txt]\n", argv[0]);
        return -1;
    }

    if (!esNumero(argv[1]) || !esNumero(argv[2])) {
        printf("ERROR: Los argumentos deben ser numeros enteros positivos.\n");
        return -1;
    }

    int n_jugadores = atoi(argv[1]);
    m_mesas = atoi(argv[2]);

    if (n_jugadores < 2 || m_mesas < 1) {
        printf("ERROR: Deben haber minimo 2 jugadores y 1 mesa\n");
        return -1;
    }

    struct Config config_juego;
    config_juego.min_time = 50;
    config_juego.max_time = 300;
    config_juego.seed = time(NULL);
    config_juego.posiciones[0] = 0; 
    for (int i = 1; i <= 12; i++) {
        config_juego.posiciones[i] = 15 + i; 
    }

    if (argc == 4) {
        leerConfig(argv[3], &config_juego);
    }

    corregirPosiciones(&config_juego);

    // calculo real de las partidas
    int partidas_posibles = n_jugadores / 2;

    printf("Iniciando servidor: %d jugadores formaran %d partidas totales (Max mesas simultaneas: %d)\n", n_jugadores, partidas_posibles, m_mesas);

    pid_mesas = malloc(m_mesas * sizeof(pid_t));
    if (pid_mesas == NULL) {
        perror("Error de memoria");
        return -1;
    }
    for(int i = 0; i < m_mesas; i++) pid_mesas[i] = 0;

    int partidas_activas = 0;
    int partidas_despachadas = 0;

    // llenar las mesas inicialmente
    for (int i = 0; i < m_mesas && partidas_despachadas < partidas_posibles; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("Error al hacer fork");
        } 
        else if (pid == 0) {
            signal(SIGINT, SIG_IGN); // aqui lo agregue para que el hijo ignore ctrl+c
            printf("[Partida %d] Iniciada en Mesa %d con PID: %d\n", partidas_despachadas + 1, i + 1, getpid());
            iniciarPartida(partidas_despachadas, &config_juego); 
        }
        else {
            pid_mesas[i] = pid; 
            partidas_activas++;
            partidas_despachadas++;
        }
    }

    while (partidas_activas > 0 && server_activo) {
        pid_t pid_terminado = wait(NULL); // espera a que cualquier mesa termine
        
        if (pid_terminado < 0) {
            if (!server_activo) break; // si el wait fallo por el ctrl+c salimos del bucle
            continue;
        }
        
        if (pid_terminado > 0) {
            partidas_activas--;

            // busca que mesa se desocupo
            int mesa_liberada = -1;
            for (int i = 0; i < m_mesas; i++) {
                if (pid_mesas[i] == pid_terminado) {
                    pid_mesas[i] = 0; 
                    mesa_liberada = i;
                    break;
                }
            }

            // mete los jugadores en espera a la mesa libre
            if (partidas_despachadas < partidas_posibles && mesa_liberada != -1 && server_activo) {
                pid_t pid = fork();

                if (pid < 0) {
                    perror("Error al hacer fork");
                } 
                else if (pid == 0) {
                    signal(SIGINT, SIG_IGN); // el hijo ignora ctrl+c
                    printf("[Partida %d] Iniciada en Mesa %d (Reciclada) con PID: %d\n", partidas_despachadas + 1, mesa_liberada + 1, getpid());
                    iniciarPartida(partidas_despachadas, &config_juego);
                } 
                else {
                    pid_mesas[mesa_liberada] = pid; 
                    partidas_activas++;
                    partidas_despachadas++;
                }
            }
        }
    }

    if (!server_activo) {
        printf("\nInterrumpiendo Servidor. Cerrando todas las mesas.\n");
        for(int i = 0; i < m_mesas; i++){
            if(pid_mesas[i] > 0){
                kill(pid_mesas[i], SIGTERM); //le manda la senal a las mesas
            }
        }
    }

    for(int i = 0; i < m_mesas; i++){
        if(pid_mesas[i] > 0){
            waitpid(pid_mesas[i], NULL, 0); //espera a que terminen
        }
    }

    free(pid_mesas);

    if (server_activo) {
        printf("Torneo terminado. %d partidas ejecutadas exitosamente.\n", partidas_posibles);
    } else {
        printf("CTRL + C. Partidas Terminadas.\n");
    }
    
    return 0;
}