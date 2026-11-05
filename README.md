# Torneo de Komikan Concurrente

· Sebastián Saldivia, Martín Correa

Servidor de torneo para el Komikan: un puma contra doce perros en un tablero de 28 casillas, con todas las fichas moviéndose de forma simultánea.

## Compilación y ejecución

```bash
gcc -Wall -Wextra -std=c17 -pthread server.c mesa.c -o torneo
./torneo <N_jugadores> <M_mesas> [config.txt]
```

- `N_jugadores`: cantidad de jugadores (mínimo 2). Se emparejan en parejas puma/perros.
- `M_mesas`: máximo de mesas en paralelo (mínimo 1). Los demás esperan en cola.
- `config.txt` (opcional), con formato `clave:valor`:

```
seed:432432432
min_time:50
max_time:300
positions:10,2,5,6,7,3,9,0,8,20,12,18,23
```

`min_time` y `max_time` van entre 50 y 300 ms y `min_time` debe ser menor o igual a `max_time`. En `positions`, el primer valor es el puma y los otros 12 son los perros.

## Diseño

- **Arquitectura:** el servidor empareja jugadores y crea un proceso hijo (`fork()`) por mesa, hasta `M` a la vez. En cada mesa, el hilo principal es el juez y hay 13 hilos de ficha (1 puma y 12 perros).
- **Sincronización:** un `mutex` por casilla (28), un `lock_juez` y banderas `atomic_int`. Mover una ficha ocurre en una sola sección crítica; una captura toma tres *locks* y revalida las condiciones. Todos los *locks* se piden en orden ascendente de casilla.
- **Juez:** revisa el estado cada 20 ms y detecta la victoria del puma (6 capturas), la de los perros (puma sin movimientos) y el abandono.
- **Señales:** `SIGUSR1` y `SIGUSR2` simulan el abandono. Ante Ctrl+C, el servidor envía `SIGTERM` a las mesas, las espera con `waitpid` y libera su memoria.

## Riesgos de concurrencia

- **Deadlock:** evitado con el orden total de *locks*.
- **Condición de carrera:** toda decisión se revalida con los *locks* tomados.
- **Livelock:** no hay reintentos ni retrocesos; un movimiento fallido espera al siguiente ciclo.
- **Starvation:** las secciones críticas duran microsegundos y ningún hilo duerme con un *lock* tomado.

## Evaluación y pruebas

- Un *mutex* por casilla permite más paralelismo que uno por tablero, a cambio de más complejidad.
- Un proceso por mesa da aislamiento y permite detectar caídas con `waitpid`.
- El sondeo del juez cada 20 ms es más simple que usar variables de condición.
- Compila sin advertencias.
- Los jugadores se simulan. Como las fichas se mueven al azar, casi siempre gana el puma.