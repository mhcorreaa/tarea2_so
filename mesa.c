#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "mesa.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h> // ctrl + c

struct Mesa *mesa_hijo_ptr = NULL;

void detener(int sig){
    (void)sig;
    if(mesa_hijo_ptr != NULL){
        mesa_hijo_ptr->juego_activo = false; // esto saca a los hilos de su ciclo y al juez
    }
}

void agregarVecino(struct Casilla *c, int id_vecino) {
    if (c->cant_vecinos < 8) {
        c->id_vecinos[c->cant_vecinos] = id_vecino;
        c->cant_vecinos++;
    }
}

void mapearVecinos(struct Mesa *mesa) {
    // conexiones para la parte de 5*5 de la cuadricula (va desde el indice 3 al 27)
    for (int fila = 0; fila < 5; fila++) {
        for (int col = 0; col < 5; col++) {
            int actual = 3 + (fila * 5) + col;

            // conexiones perpendiculares
            if (col > 0) agregarVecino(&mesa->casilla[actual], actual - 1); //izquierda
            if (col < 4) agregarVecino(&mesa->casilla[actual], actual + 1);  //derecha
            if (fila > 0) agregarVecino(&mesa->casilla[actual], actual - 5);  //arriba
            if (fila < 4) agregarVecino(&mesa->casilla[actual], actual + 5);  //abajo

            //conexiones diagonales (solo en intersecciones donde fila+col es par)
            if ((fila + col) % 2 == 0) {
                if (fila > 0 && col > 0) agregarVecino(&mesa->casilla[actual], actual - 6);
                if (fila > 0 && col < 4) agregarVecino(&mesa->casilla[actual], actual - 4);  //OJO comentado mientras el ayudante responde 
                if (fila < 4 && col > 0) agregarVecino(&mesa->casilla[actual], actual + 4);
                if (fila < 4 && col < 4) agregarVecino(&mesa->casilla[actual], actual + 6);
            }
        }
    }

    // conexiones manuales en la parte asimetrica del trono
    agregarVecino(&mesa->casilla[0], 1);
    agregarVecino(&mesa->casilla[0], 2);
    agregarVecino(&mesa->casilla[0], 5); 
    
    agregarVecino(&mesa->casilla[1], 0);
    agregarVecino(&mesa->casilla[1], 2);
    agregarVecino(&mesa->casilla[1], 3); 
    //agregarVecino(&mesa->casilla[1], 5); 

    agregarVecino(&mesa->casilla[2], 0);
    agregarVecino(&mesa->casilla[2], 1);
    //agregarVecino(&mesa->casilla[2], 5);
    agregarVecino(&mesa->casilla[2], 7); 

    // conexiones fila 2 hacia triangulo
    agregarVecino(&mesa->casilla[3], 1);
    agregarVecino(&mesa->casilla[5], 0);
    //agregarVecino(&mesa->casilla[5], 1);
    //agregarVecino(&mesa->casilla[5], 2);
    agregarVecino(&mesa->casilla[7], 2);
}

void inicializarMesa(struct Mesa *mesa) {
    mesa->juego_activo = 1; //lo cambie a uno por el sig_atomic_t
    mesa->perros_capturados = 0;
    mesa->estado = 1; // ocupada

    if (pthread_mutex_init(&mesa->lock_juez, NULL) != 0) {
        perror("error al inicializar el lock_juez");
        exit(EXIT_FAILURE);

    for (int i = 0; i < 13; i++) mesa->viva[i] = true;

    for (int i = 0; i < num_casillas; i++) {
        mesa->casilla[i].nro_casilla = i;
        mesa->casilla[i].ocupante = libre;
        mesa->casilla[i].cant_vecinos = 0;
        mesa->casilla[i].id_ficha = -1;
        
        if (pthread_mutex_init(&mesa->casilla[i].lock, NULL) != 0) {
            perror("error al inicializar el mutex de la casilla");
            exit(EXIT_FAILURE);
        }
    }

    mapearVecinos(mesa);

    // asignar ocupantes 
    mesa->casilla[mesa->config.posiciones[0]].ocupante = puma;
    mesa->casilla[mesa->config.posiciones[0]].id_ficha = 0;
    
    for (int i = 1; i <= 12; i++) { 
        mesa->casilla[mesa->config.posiciones[i]].ocupante = perro;
        mesa->casilla[mesa->config.posiciones[i]].id_ficha = i;
    }
}

int calcularSalto(int origen, int medio) {
    if (origen == 0 && medio == 1) return 3;
    if (origen == 3 && medio == 1) return 0;
    if (origen == 0 && medio == 2) return 7;        //logica de salto en el triangulo
    if (origen == 7 && medio == 2) return 0;
    if (origen == 0 && medio == 5) return 10;
    if (origen == 10 && medio == 5) return 0;
    if((origen == 1 && medio == 2) || (origen == 2 && medio == 1)) return -1;  //saltos no posibles

    int delta = medio - origen;
    int destino = medio + delta;

    if (destino < 3 || destino >= num_casillas) return -1;
    
    int col_origen = (origen - 3) % 5;
    int col_destino = (destino - 3) % 5;
    if (abs(col_origen - col_destino) > 2) return -1; 

    return destino;
}

void *rutinaJugador(void *arg){

    struct infojugador *info = (struct infojugador *)arg;
    struct Mesa *mesa = info->mesa;
    
    // semilla combinada para asegurar reproducibilidad y variacion entre hilos
    unsigned int seed = mesa->config.seed + info->id_jugador;
    
    int min_t = mesa->config.min_time;
    int max_t = mesa->config.max_time;

    while (mesa->juego_activo) {
        // retardo dinamico
        int ms_retardo = (rand_r(&seed) % (max_t - min_t + 1)) + min_t;
        usleep(ms_retardo * 1000);

        if (!mesa->juego_activo) break;

        int pos_actual = info->pos_actual;
        if (mesa->casilla[pos_actual].cant_vecinos == 0) continue;
        
        int indice_vecino = rand_r(&seed) % mesa->casilla[pos_actual].cant_vecinos; //genera indice aleatorio para cambiarse de casilla entre los vecinos
        int vecino = mesa->casilla[pos_actual].id_vecinos[indice_vecino];

        int destino_salto = -1;
        if (info->tipo_ficha == puma && mesa->casilla[vecino].ocupante == perro) { //siendo puma, ve la posibilidad de comerse un perro
            destino_salto = calcularSalto(pos_actual, vecino);
        }

        if (destino_salto != -1) {
            // regla evita deadlock 3 casillas puma
            int ids[3] = {pos_actual, vecino, destino_salto};
            for (int x = 0; x < 2; x++) { //bubble sort, ordena de menor a mayor
                for (int y = 0; y < 2 - x; y++) {
                    if (ids[y] > ids[y+1]) {
                        int temp = ids[y];
                        ids[y] = ids[y+1];
                        ids[y+1] = temp;
                    }
                }
            }

            pthread_mutex_lock(&mesa->casilla[ids[0]].lock);
            pthread_mutex_lock(&mesa->casilla[ids[1]].lock);
            pthread_mutex_lock(&mesa->casilla[ids[2]].lock);

            if (mesa->casilla[pos_actual].ocupante == puma && 
                mesa->casilla[vecino].ocupante == perro && 
                mesa->casilla[destino_salto].ocupante == libre) { //puma se come al perro
                
                int id_capturado = mesa->casilla[vecino].id_ficha;
                mesa->viva[id_capturado] = false; // el hilo de ese perro termina en su proximo intento de movimiento

                mesa->casilla[vecino].ocupante = libre; 
                mesa->casilla[vecino].id_ficha = -1;
                mesa->casilla[destino_salto].ocupante = puma;
                mesa->casilla[destino_salto].id_ficha = info->id_jugador;
                mesa->casilla[pos_actual].ocupante = libre;
                mesa->casilla[pos_actual].id_ficha = -1;
                
                pthread_mutex_lock(&mesa->lock_juez);
                info->pos_actual = destino_salto;
                mesa->perros_capturados++; 
                pthread_mutex_unlock(&mesa->lock_juez);
                
                printf("\nEl puma salto y se comio al perro %d cayendo en %d, total capturados: %d\n", id_capturado, destino_salto, mesa->perros_capturados);
            }

            pthread_mutex_unlock(&mesa->casilla[ids[2]].lock);
            pthread_mutex_unlock(&mesa->casilla[ids[1]].lock);
            pthread_mutex_unlock(&mesa->casilla[ids[0]].lock);
        } 
        else {
            // movimiento normal
            int id_primero;
            int id_segundo;

            if (pos_actual < vecino) { //se ordena de menor a mayor
                id_primero = pos_actual;
                id_segundo = vecino;
            } else {
                id_primero = vecino;
                id_segundo = pos_actual;
            }

            pthread_mutex_lock(&mesa->casilla[id_primero].lock);
            pthread_mutex_lock(&mesa->casilla[id_segundo].lock);

            if (!mesa->viva[info->id_jugador]) { // la ficha fue capturada, deja de jugar
                pthread_mutex_unlock(&mesa->casilla[id_segundo].lock);
                pthread_mutex_unlock(&mesa->casilla[id_primero].lock);
                break;
            }

            if (mesa->casilla[vecino].ocupante == libre) {
                mesa->casilla[vecino].ocupante = info->tipo_ficha;
                mesa->casilla[vecino].id_ficha = info->id_jugador;
                mesa->casilla[pos_actual].ocupante = libre;
                mesa->casilla[pos_actual].id_ficha = -1;
                
                pthread_mutex_lock(&mesa->lock_juez);
                info->pos_actual = vecino;
                pthread_mutex_unlock(&mesa->lock_juez);
            }


            pthread_mutex_unlock(&mesa->casilla[id_segundo].lock);
            pthread_mutex_unlock(&mesa->casilla[id_primero].lock);
        }
    }
    return NULL;
}

void imprimirTablero(struct Mesa *mesa, int id_mesa) {
    char rep[num_casillas];
    
    // Lectura rapida y segura del tablero
    for (int i = 0; i < num_casillas; i++) {
        pthread_mutex_lock(&mesa->casilla[i].lock);
        if (mesa->casilla[i].ocupante == puma) rep[i] = 'U'; 
        else if (mesa->casilla[i].ocupante == perro) rep[i] = 'P'; 
        else rep[i] = '-'; 
        pthread_mutex_unlock(&mesa->casilla[i].lock);
    }

    // Impresion basica en formato de texto plano
    printf("\n--- Tablero Mesa %d ---\n", id_mesa + 1);
    printf("Trono: [%c]\n", rep[0]);
    printf("Nodos asimetricos: [%c] [%c]\n", rep[1], rep[2]);
    printf("Cuadricula:\n");
    
    for (int fila = 0; fila < 5; fila++) {
        for (int col = 0; col < 5; col++) {
            printf("[%c] ", rep[3 + (fila * 5) + col]);
        }
        printf("\n");
    }
    printf("-----------------------\n\n");
}

void iniciarPartida(int id_mesa, struct Config *config_juego) {
    struct Mesa mesa;
    mesa.config = *config_juego; // asigna config a la mesa 
    
    mesa_hijo_ptr = &mesa;
    signal(SIGTERM, detener);

    inicializarMesa(&mesa);

    pthread_t hilos[13];
    struct infojugador info_hilos[13];

    // hilo puma
    info_hilos[0].id_jugador = 0;
    info_hilos[0].tipo_ficha = puma;
    info_hilos[0].pos_actual = mesa.config.posiciones[0]; // seteo desde archivo
    info_hilos[0].mesa = &mesa;
    pthread_create(&hilos[0], NULL, rutinaJugador, (void *)&info_hilos[0]);

    // 12 hilos perros
    for (int j = 1; j <= 12; j++) {
        info_hilos[j].id_jugador = j;
        info_hilos[j].tipo_ficha = perro;
        info_hilos[j].pos_actual = mesa.config.posiciones[j]; // seteo desde archivo
        info_hilos[j].mesa = &mesa;
        pthread_create(&hilos[j], NULL, rutinaJugador, (void *)&info_hilos[j]);
    }

    // funcionamiento juez
    while (mesa.juego_activo) {
        usleep(500000); // revisa cada 500 ms

        imprimirTablero(&mesa, id_mesa);

        // aca bloqueamos y copiamos los valores a variables locales y desbloqueamos al instante
        pthread_mutex_lock(&mesa.lock_juez);
        int capturados_seguro = mesa.perros_capturados;
        int pos_puma = info_hilos[0].pos_actual;
        pthread_mutex_unlock(&mesa.lock_juez);

        // victoria puma (ahora usamos la copia segura)
        if (capturados_seguro >= 6) {
            printf("\n[Juez Mesa %d] Gana el PUMA ya que capturo 6 perros.\n", id_mesa + 1);
            mesa.juego_activo = 0;
            break;
        }

        // victoria de los perros (pos_puma ya fue leida de forma segura arriba)
        int cant_vecinos = mesa.casilla[pos_puma].cant_vecinos;
        
        // agrupar casilla actual + todos los vecinos + casillas de aterrizaje
        int ids_bloqueo[17]; 
        int total_locks = 0;
        ids_bloqueo[total_locks++] = pos_puma;

        for (int v = 0; v < cant_vecinos; v++) {

            int vecino = mesa.casilla[pos_puma].id_vecinos[v];
            ids_bloqueo[total_locks++] = vecino;

            int salto = calcularSalto(pos_puma, vecino);
            
            if (salto != -1) ids_bloqueo[total_locks++] = salto;
        }

        // ordenar arreglo de menor a mayor (bubble sort)
        for (int x = 0; x < total_locks - 1; x++) {
            for (int y = 0; y < total_locks - x - 1; y++) {
                if (ids_bloqueo[y] > ids_bloqueo[y + 1]) {
                    int temp = ids_bloqueo[y];
                    ids_bloqueo[y] = ids_bloqueo[y + 1];
                    ids_bloqueo[y + 1] = temp;
                }
            }
        }

        // asegurar mutex en cada una de las casillas
        for (int x = 0; x < total_locks; x++){

            if (x > 0 && ids_bloqueo[x] == ids_bloqueo[x - 1]) continue;
            pthread_mutex_lock(&mesa.casilla[ids_bloqueo[x]].lock);
        }

        bool puma_se_movio = (mesa.casilla[pos_puma].ocupante != puma);
        bool puma_atrapado = false;

        if (!puma_se_movio){

            puma_atrapado = true;

            for (int v = 0; v < cant_vecinos; v++) { //el puma no tiene movimientos si no hay vecino libre ni salto posible

                int destino = mesa.casilla[pos_puma].id_vecinos[v];

                if (mesa.casilla[destino].ocupante == libre) {
                    puma_atrapado = false;
                    break;
                }

                int salto = calcularSalto(pos_puma, destino); //vecino con perro, ve si puede comerselo

                if (salto != -1 && mesa.casilla[salto].ocupante == libre) {
                    puma_atrapado = false;
                    break;
                }
            }
        }
        
        // liberar mutex en orden inverso
        for (int x = total_locks - 1; x >= 0; x--) {
            if (x > 0 && ids_bloqueo[x] == ids_bloqueo[x - 1]) continue;
            pthread_mutex_unlock(&mesa.casilla[ids_bloqueo[x]].lock);
        }

        if (puma_se_movio) continue; // posicion vieja, se revisa en la siguiente vuelta

        if (puma_atrapado) {
            printf("\n[Juez Mesa %d] Ganan los PERROS, el puma esta acorralado.\n", id_mesa + 1);
            mesa.juego_activo = false;
            break;
        }
    }

    // esperar hilos y limpiar memoria antes de morir 
    for (int j = 0; j < 13; j++) {
        pthread_join(hilos[j], NULL);
    } 
    for(int i = 0; i < num_casillas; i++){
        pthread_mutex_destroy(&mesa.casilla[i].lock);
    }

    pthread_mutex_destroy(&mesa.lock_juez);
    
    printf("[Mesa %d] Finalizada limpiamente.\n", id_mesa + 1);
    exit(EXIT_SUCCESS); 
}