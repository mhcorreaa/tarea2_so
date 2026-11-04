CC = gcc
CFLAGS = -Wall -Wextra -std=c17
LDFLAGS = -pthread

torneo: server.c mesa.c mesa.h
	$(CC) $(CFLAGS) -o torneo server.c mesa.c $(LDFLAGS)

clean:
	rm -f torneo

.PHONY: clean
