CC = gcc
CFLAGS = -Wextra -pthread -lrt

all: Client Serveur



Client: Client.c
	$(CC) $(CFLAGS) -o Client Client.c

Serveur: Serveur.c
	$(CC) $(CFLAGS) -o Serveur Serveur.c

	
