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
#include <errno.h>

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

void sigchldVacio(int sig){ // esto no hace nada solo existe para que SIGCHLD despierte al sigsuspend del servidor
    (void)sig;
}

// simula la llegada de los n jugadores de manera aleatoria y los empareja un jugador puma 
int armarParejas(int n_jugadores, unsigned int seed, int *pareja_puma, int *pareja_perros) {

    int *roles = malloc(n_jugadores * sizeof(int));
    int *espera_puma = malloc(n_jugadores * sizeof(int));   // ids de jugadores puma esperando oponente
    int *espera_perros = malloc(n_jugadores * sizeof(int)); // ids de jugadores perros esperando oponente

    if (roles == NULL || espera_puma == NULL || espera_perros == NULL) {
        perror("Error de memoria");
        free(roles);
        free(espera_puma);
        free(espera_perros);
        return -1;
    }

    for (int i = 0; i < n_jugadores; i++) roles[i] = (i % 2 == 0) ? puma : perro; 

    for (int i = n_jugadores - 1; i > 0; i--) { 
        int j = rand_r(&seed) % (i + 1);
        int temp = roles[i];
        roles[i] = roles[j];
        roles[j] = temp;
    }

    int n_puma = 0, n_perros = 0, parejas = 0;

    for (int i = 0; i < n_jugadores && server_activo; i++) { // si llega ctrl + c dejan de llegar jugadores

        usleep((rand_r(&seed) % 5) * 1000); // los jugadores no llegan todos al mismo tiempo

        int id_jugador = i + 1;
        printf("[Matchmaker] Llega el jugador %d como %s\n", id_jugador, roles[i] == puma ? "PUMA" : "PERROS");

        if (roles[i] == puma) espera_puma[n_puma++] = id_jugador;
        else espera_perros[n_perros++] = id_jugador;

        if (n_puma > 0 && n_perros > 0) { // ya hay un puma y un perros esperando: se emparejan
            pareja_puma[parejas] = espera_puma[--n_puma];
            pareja_perros[parejas] = espera_perros[--n_perros];
            printf("pareja %d: jugador %d puma vs jugadores %d perros\n", parejas + 1, pareja_puma[parejas], pareja_perros[parejas]);
            parejas++;
        }
    }

    if (server_activo && n_jugadores - (2 * parejas) > 0) {
        printf("jugadores que quedan sin pareja y no jugaran \n", n_jugadores - (2 * parejas));
    }

    free(roles);
    free(espera_puma);
    free(espera_perros);
    return parejas;
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

    int temp_min_time = config_juego->min_time, temp_max_time = config_juego->max_time;

    while (fgets(buffer, sizeof(buffer), read) != NULL){

        char *linea = buffer;
        
        char *clave = strsep(&linea, ":");
        char *valor = strsep(&linea, "\n"); 
        
        if (valor != NULL) {
            valor += strspn(valor, " \t"); //ignora espacios antes del valor
            size_t largo = strlen(valor);
            while (largo > 0 && (valor[largo - 1] == ' ' || valor[largo - 1] == '\t' || valor[largo - 1] == '\r')) valor[--largo] = '\0';
        }

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

            else printf("Advertencia: linea '%s' ignorada (clave desconocida o valor invalido).\n", clave);
        }
    }

    fclose(read);

    if(temp_min_time >= 0 && temp_min_time <= temp_max_time){ //valida que el tiempo de espera sea valido antes de ingresarlo

        config_juego->min_time = temp_min_time;
        config_juego->max_time = temp_max_time;
    }
    else {
        printf("tiempos invalidos (min %dy max %d) por lo que se usan los predeterminados.\n", temp_min_time, temp_max_time);
    }

    if (config_juego->min_time < 50 || config_juego->max_time > 300) {
        printf("los retardos deben estar entre 50 y 300 ms\n", config_juego->min_time, config_juego->max_time);
    }

}

int main(int argc, char *argv[]){

    setvbuf(stdout, NULL, _IOLBF, 0); // salida por lineas pq si no los hijos del fork repiten lo que quedo en el buffer

    struct sigaction sa;
    sa.sa_handler = detenerJuego;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // sin SA_RESTART para que interrumpa el wait
    sigaction(SIGINT, &sa, NULL);

    if (argc < 3 || argc > 4) {
        printf("uso %s <N_jugadores> <M_mesas> [config.txt]\n", argv[0]);
        return -1;
    }

    if (!esNumero(argv[1]) || !esNumero(argv[2])) {
        printf("los argumentos deben ser numeros enteros positivos.\n");
        return -1;
    }

    int n_jugadores = atoi(argv[1]);
    m_mesas = atoi(argv[2]);

    if (n_jugadores < 2 || m_mesas < 1) {
        printf("deben haber minimo 2 jugadores y 1 mesa\n");
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

    int partidas_posibles = n_jugadores / 2;

    printf("cargando el servidor %d jugadores formaran %d partidas totales%d)\n", n_jugadores, partidas_posibles, m_mesas);

    pid_mesas = malloc(m_mesas * sizeof(pid_t));
    if (pid_mesas == NULL) {
        perror("error de memoria");
        return -1;
    }
    for(int i = 0; i < m_mesas; i++) pid_mesas[i] = 0;

    int *pareja_puma = malloc((partidas_posibles + 1) * sizeof(int));
    int *pareja_perros = malloc((partidas_posibles + 1) * sizeof(int));
    if (pareja_puma == NULL || pareja_perros == NULL) {
        perror("error de memoria");
        free(pareja_puma);
        free(pareja_perros);
        free(pid_mesas);
        return -1;
    }

    // llegada y emparejamiento de los jugadores
    partidas_posibles = armarParejas(n_jugadores, config_juego.seed, pareja_puma, pareja_perros);
    if (partidas_posibles < 0) {
        free(pareja_puma);
        free(pareja_perros);
        free(pid_mesas);
        return -1;
    }

    // SIGINT y SIGCHLD quedan bloqueadas y solo se reciben dentro de sigsuspend para que asi un ctrl + c nunca se pierde justo antes de esperar
    struct sigaction sc;
    sc.sa_handler = sigchldVacio;
    sigemptyset(&sc.sa_mask);
    sc.sa_flags = 0;
    sigaction(SIGCHLD, &sc, NULL);

    sigset_t bloqueadas, vacia;
    sigemptyset(&bloqueadas);
    sigaddset(&bloqueadas, SIGINT);
    sigaddset(&bloqueadas, SIGCHLD);
    sigemptyset(&vacia);
    sigprocmask(SIG_BLOCK, &bloqueadas, NULL);

    int partidas_activas = 0;
    int partidas_despachadas = 0;

    // llenar las mesas inicialmente
    for (int i = 0; i < m_mesas && partidas_despachadas < partidas_posibles && server_activo; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("Error al hacer fork");
        } 
        else if (pid == 0) {
            signal(SIGINT, SIG_IGN); // aqui lo agregue para que el hijo ignore ctrl+c
            sigprocmask(SIG_SETMASK, &vacia, NULL); // el hijo hereda las senales bloqueadas se desbloquean
            printf("[Partida %d] Iniciada en Mesa %d con PID: %d\n", partidas_despachadas + 1, i + 1, getpid());
            iniciarPartida(i, partidas_despachadas, &config_juego, pareja_puma[partidas_despachadas], pareja_perros[partidas_despachadas]); 
        }
        else {
            pid_mesas[i] = pid; 
            partidas_activas++;
            partidas_despachadas++;
        }
    }

    while (partidas_activas > 0 && server_activo) {
        int estado = 0;
        pid_t pid_terminado = waitpid(-1, &estado, WNOHANG); // revisa si alguna mesa termino sin bloquearse

        if (pid_terminado == 0) {
            sigsuspend(&vacia); // aca se desbloquea SIGINT y SIGCHLD de forma atomica y duerme hasta que llegue alguna
            continue;
        }
        
        if (pid_terminado < 0) {
            if (errno == EINTR) continue;
            break; // no quedan hijos que esperar
        }
        
        if (pid_terminado > 0) {
            partidas_activas--;

            if (!(WIFEXITED(estado) && WEXITSTATUS(estado) == 0)) { // la mesa no termino con exit(0): se cayo el proceso de la partida
                printf("la mesa del proceso %d termino la partida\n", pid_terminado);
            }

            // busca que mesa esta desocupada
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
                    sigprocmask(SIG_SETMASK, &vacia, NULL);
                    printf("[Partida %d] Iniciada en Mesa %d (Reciclada) con PID: %d\n", partidas_despachadas + 1, mesa_liberada + 1, getpid());
                    iniciarPartida(mesa_liberada, partidas_despachadas, &config_juego, pareja_puma[partidas_despachadas], pareja_perros[partidas_despachadas]);
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
        printf("\ninterrumpiendo servidor se cierran todas las mesas\n");
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
    free(pareja_puma);
    free(pareja_perros);

    if (server_activo) {
        printf("torneo terminado %d partidas ejecutadas\n", partidas_despachadas);
    } else {
        printf("CTRL + C las partidas terminaron\n");
    }
    
    return 0;
}