#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h> // Ajout de l'inclusion de errno.h
#include <semaphore.h>
#include <dirent.h>

#define TUBE_DEMANDE "/tmp/tube_demande"
#define BUFFER_SIZE 265
char buffer[BUFFER_SIZE];


// Déclaration des sémaphores globales
sem_t *read_semaphore;
sem_t *write_semaphore;

//------------------------------------------------------Partage de la demande de question----------------------------------------------

void send_question_to_server() {
    int fd_demande = open(TUBE_DEMANDE, O_WRONLY);
    if (fd_demande == -1) {
        perror("Erreur lors de l'ouverture du tube de demande");
        exit(EXIT_FAILURE);
    }

    pid_t pid = getpid();

    // Suppression du saut de ligne de la fin de la question
    char question[BUFFER_SIZE];
    snprintf(question, BUFFER_SIZE, "%d", (int)pid); // Convertir pid en chaîne de caractères
    size_t len = strlen(question);

    // Envoi de la question au serveur
    write(fd_demande, question, len + 1); // Utilisation de len au lieu de strlen(question)

    // Fermeture du descripteur de fichier du tube de demande
    close(fd_demande);
}

//------------------------------------------------------Reception de la reponse----------------------------------------------

void receive_response_from_server() {
    char TUBE_REPONSE[BUFFER_SIZE];
    snprintf(TUBE_REPONSE, BUFFER_SIZE, "/tmp/%d", getpid());

    // Création du tube de réponse
    mkfifo(TUBE_REPONSE, 0666);
    int FD_REPONSE = open(TUBE_REPONSE, O_RDONLY);

    while (1) {
        // Attente d'une réponse
        ssize_t bytes_read = read(FD_REPONSE, buffer, BUFFER_SIZE);

        if (bytes_read == 0) {
            // EOF détecté, le client s'est terminé, mais le serveur continue
            continue;
        } else {
	    // Traitement normal de la réponse
	    if(strcmp("RST",buffer) != 0){
	    printf("Client:\tReponse reçu du tube %s \n\tThreads n°%s attribué\n\tEnvoi de la commande dans l'espace partagée accordée\n", TUBE_REPONSE, buffer);
	    }else{printf("\tAucun Thread disponible : Arrêt");
	    }
    break;
        }
    }

    // Fermeture du tube de réponse
    close(FD_REPONSE);
    unlink(TUBE_REPONSE);
}
//------------------------------------------------------SHM----------------------------------------------

int SHM_PARTAGEE() {
    int shm_fd;
    void *ptr;
    struct stat shm_stat;
    
 char shm_name[50];
 sprintf(shm_name, "Memory_Thread_%s", buffer); // +1 pour le caractère nul
 
	while (1) {
	// Ouvrir les sémaphores
	char semaphore_read_name[BUFFER_SIZE];
	char semaphore_write_name[BUFFER_SIZE];
	snprintf(semaphore_read_name, BUFFER_SIZE, "/Read_%s", buffer);
	snprintf(semaphore_write_name, BUFFER_SIZE, "/Write_%s", buffer);

	read_semaphore = sem_open(semaphore_read_name, 0);
	write_semaphore = sem_open(semaphore_write_name, 0);

	// Vérifier si les sémaphores sont ouverts avec succès
	if (read_semaphore != SEM_FAILED && write_semaphore != SEM_FAILED) {
	break;  // Sortir de la boucle si les sémaphores sont ouverts
	} else {
	perror("sem_open");
	// Attendre un court instant avant de réessayer
	sleep(1);
	}
	}

			    
 // Attendre que la mémoire partagée soit créée
    do {
        // Ouvrir la mémoire partagée
        shm_fd = shm_open(shm_name, O_RDWR, 0666);
        if (shm_fd == -1) {
            usleep(10000); // Attendre 10 millisecondes avant de vérifier à nouveau
        }
    } while (shm_fd == -1);

    // Mapper la mémoire partagée dans l'espace d'adresse du processus
    ptr = mmap(0, 1024, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED) {
        perror("Erreur lors du mappage de la mémoire partagée");
        exit(EXIT_FAILURE);
    }
    // Obtenir les informations sur la mémoire partagée
    if (fstat(shm_fd, &shm_stat) == -1) {
        perror("fstat");
        return 1;
    }


// Lire et écrire des données dans la mémoire partagée
char previous_value[1024];

while (1) {

		memset(ptr, 0, shm_stat.st_size);
	printf("\tSaisissez la commande à réaliser par le thread : ");
	fgets((char *)ptr, 1024, stdin);

	// Supprimer le dernier caractère s'il s'agit d'un retour à la ligne
	size_t len = strlen((char *)ptr);
	if (len > 0 && ((char *)ptr)[len - 1] == '\n') {
	((char *)ptr)[len - 1] = '\0';
	}

	if (strcmp("END", (char *)ptr) == 0 || strcmp("END_Serveur", (char *)ptr) == 0 || strcmp("END_serveur", (char *)ptr) == 0 || strcmp("end_serveur", (char *)ptr) == 0) {
	sem_post(write_semaphore);
	break;
	}

	sem_post(write_semaphore);
	sem_wait(read_semaphore);

	// The value has changed, print the message and update previous_value
	printf("\tRésultats de la commande : \n %s\n\n", (char *)ptr);
	strcpy(previous_value, (char *)ptr);

}




    return 0;
}
//------------------------------------------------------Main----------------------------------------------

int main() {
    // Envoi d'une question au serveur
    send_question_to_server();

    // Réception de la réponse du serveur
    receive_response_from_server();
    
    
   //Création de la mémoire partagé
   if(strcmp("RST",buffer) != 0){SHM_PARTAGEE();
   } 
    return 0;
}
