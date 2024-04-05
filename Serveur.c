#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <semaphore.h>
#include <sys/mman.h>

#define CONFIG_FILE "Demon.conf"
#define TUBE_DEMANDE "/tmp/tube_demande"
#define BUFFER_SIZE 265

///Reste Sémaphores + Thread initalisation min thread

int min_thread, max_thread, max_connect_per_thread, shm_size;
volatile int Crucifix = 0;
int thread_created[200];
int thread_dispo[200];
	

//---------------------------------------------------------------------Demon.conf-----------------------------------------------------

void read_config() {
    FILE *config_file = fopen(CONFIG_FILE, "r");
    if (config_file == NULL) {
        perror("Erreur : Impossible d'ouvrir le fichier de configuration");
        exit(EXIT_FAILURE);
    }

    if (fscanf(config_file, " MIN_THREAD = %d", &min_thread) != 1 ||
        fscanf(config_file, " MAX_THREAD = %d", &max_thread) != 1 ||
        fscanf(config_file, " MAX_CONNECT_PER_THREAD = %d", &max_connect_per_thread) != 1 ||
        fscanf(config_file, " SHM_SIZE = %d", &shm_size) != 1) {
        perror("Erreur : Impossible de lire les paramètres du fichier de configuration");
        exit(EXIT_FAILURE);
    }

    fclose(config_file);
    
    printf("Serveur : \tLecture fichier %s terminé\n", CONFIG_FILE);
    
    if(max_connect_per_thread == 0){printf("Serveur : \tParmétre non incorporé : max_connect_per_thread = 0\n");
    }
}

//---------------------------------------------------------------------CTRL+C-----------------------------------------------------

void handle_sigint(int sig) {
    printf("\nServeur :\tCtrl+C détecté. Arrêt du programme.\n");
    Crucifix = 1;
}

//---------------------------------------------------------------------Threads-----------------------------------------------------

void *thread_function(void *args) {

    int thread_id = *((int *)args);
    char thread_id_V2[10];
    sprintf(thread_id_V2, "%d", thread_id);

    char chaine[] = "/Memory_Thread_";
    char shm_name[strlen(chaine) + strlen(thread_id_V2) + 1];
    sprintf(shm_name, "%s%s", chaine, thread_id_V2);

    int shm_fd;
    void *ptr;

		    char semaphore_read_V2[20];
		    char semaphore_write_V2[20];
		    sprintf(semaphore_read_V2, "/Read_%d", thread_id);
		    sprintf(semaphore_write_V2, "/Write_%d", thread_id);

		    // Création des sémaphores READ et WRITE avec les noms spécifiques à chaque thread
		    sem_t *read_semaphore = sem_open(semaphore_read_V2, O_CREAT, 0644, 0);
		    if (read_semaphore == SEM_FAILED) {
			perror("sem_open(read_semaphore)");
			exit(EXIT_FAILURE);
		    }

		    sem_t *write_semaphore = sem_open(semaphore_write_V2, O_CREAT, 0644, 0);
		    if (write_semaphore == SEM_FAILED) {
			perror("sem_open(write_semaphore)");
			exit(EXIT_FAILURE);
		    }

    shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Erreur lors de la création de la mémoire partagée");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(shm_fd, shm_size) == -1) {
        perror("Erreur lors de l'allocation de l'espace pour la mémoire partagée");
        exit(EXIT_FAILURE);
    }

    ptr = mmap(0, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED) {
        perror("Erreur lors du mappage de la mémoire partagée");
        exit(EXIT_FAILURE);
    }
    

    char previous_value[1024];
    strcpy(previous_value, "");
    
    printf("Thread %d :\tEn attente de COMMANDE\n", thread_id);

    while (Crucifix == 0) {
        
        if (sem_trywait(write_semaphore) == 0) {

		if (strcmp("END", (char *)ptr) == 0 || Crucifix == 1) {
		    printf("Thread %d :\tEND reçu\n", thread_id);
		    break;
		}

		if (strcmp("END_Serveur", (char *)ptr) == 0 || strcmp("END_serveur", (char *)ptr) == 0 || strcmp("end_serveur", (char *)ptr) == 0 || Crucifix == 1) {
		    printf("Thread %d :\tEND_Serveur reçu\n", thread_id);
		    Crucifix = 1;
		    break;
		}

		if (strcmp(previous_value, (char *)ptr) != 0 && strcmp("", (char *)ptr) != 0) {
		    printf("Thread %d :\tCommande reçu depuis le client : %s\n", thread_id, (char *)ptr);
		   	    
			    FILE *cmd_output;
			    cmd_output = popen((char *)ptr, "r");
			   	

			    char buffer[shm_size];
			    size_t total_read = 0;
			    size_t bytes_read;
			    while ((bytes_read = fread(buffer, 1, sizeof(buffer), cmd_output)) > 0) {
				memset(ptr, 0, shm_size);
				memcpy((char *)ptr + total_read, buffer, bytes_read);
				total_read += bytes_read;
			    }

			    if (total_read == 0) {
				strcpy(ptr, "Done");
			    }
			    
		 

		    pclose(cmd_output);
		    strcpy(previous_value, (char *)ptr);
		    
		    sem_post(read_semaphore);
		    printf("Thread %d :\tEn attente de COMMANDE\n", thread_id);

		}
	}
    }
    

	
	
    sem_close(read_semaphore);
    sem_close(write_semaphore);
    sem_unlink(semaphore_read_V2);
    sem_unlink(semaphore_write_V2);

    if (munmap(ptr, shm_size) == -1) {
        perror("Erreur lors du détachement de la mémoire partagée");
        exit(EXIT_FAILURE);
    }

    if (close(shm_fd) == -1) {
        perror("Erreur lors de la fermeture du descripteur de fichier de la mémoire partagée");
        exit(EXIT_FAILURE);
    }

    if (shm_unlink(shm_name) == -1) {
        perror("Erreur lors de la suppression de la mémoire partagée");
        exit(EXIT_FAILURE);
    }
    
    thread_created[thread_id] = 0;
        

	pthread_exit(NULL);
}

//---------------------------------------------------------------------MAIN-----------------------------------------------------

int main() {
	mkfifo(TUBE_DEMANDE, 0666);
	int FD_DEMANDE = open(TUBE_DEMANDE, O_RDWR | O_NONBLOCK);
	char buffer[BUFFER_SIZE];

	read_config();
	
	
    if(min_thread>=max_thread){printf("Serveur : \tParmétres invalides : Min_Thread >= Max_Thread\n");
    return 0;
    }
    if(shm_size<=5){printf("Serveur : \tParmétres invalides : Taille SHM insuffisant\n");
    return 0;
    }

	signal(SIGINT, handle_sigint);
	

	pthread_t threads[max_thread]; // Déclaration globale du tableau de threads
	
	DIR *dir;
	struct dirent *entry;
	dir = opendir("/dev/shm/");
	char Thread_Status[strlen("Memory_Thread_") + 1];
	
	if (dir == NULL) {
        perror("Erreur lors de l'ouverture du répertoire");
        exit(EXIT_FAILURE);
    }    
    
    
while (1) {

   for (int i = 0; i <= min_thread; i++) {
   if (!thread_created[i]){
                int *thread_id = malloc(sizeof(int));
                *thread_id = i;
                pthread_create(&threads[i], NULL, thread_function, (void *)thread_id);
		thread_created[i] = 1;
		thread_dispo[i] = 1;
        }
   }
        
sleep(1); 

    ssize_t bytes_read = read(FD_DEMANDE, buffer, BUFFER_SIZE);
    if (bytes_read > 0 && buffer[0] != '\0') {
        printf("Serveur :\tDemande reçue du terminal %s\n", buffer);

	char TUBE_REPONSE[BUFFER_SIZE];
	snprintf(TUBE_REPONSE, BUFFER_SIZE, "/tmp/%s", buffer);
	int FD_REPONSE = open(TUBE_REPONSE, O_WRONLY);// Ouvrir le tube de réponse en écriture

			for (int i = 0; i <= max_thread; i++) {
				if(i==max_thread){
				char thread_nb[20] = "";
				sprintf(thread_nb, "RST");
				write(FD_REPONSE, thread_nb, strlen(thread_nb));
				printf("\t\tRéponse du tube %s ---> Max_Thread atteint : %s \n", TUBE_REPONSE,thread_nb);
				thread_dispo[i] = 0;
				break;
			    }
			    
			    if (thread_dispo[i]) {
				char thread_nb[20] = "";
				sprintf(thread_nb, "%d", i);
				write(FD_REPONSE, thread_nb, strlen(thread_nb));
				printf("\t\tRéponse du tube %s ---> Thread n°%s \n", TUBE_REPONSE,thread_nb);
				thread_dispo[i] = 0;
				
				//printf("\t--%d--\n", thread_dispo);
				
				break;
			    }else{
				     if (!thread_created[i]) {
					int *thread_id = malloc(sizeof(int));
					*thread_id = i;
					pthread_create(&threads[i], NULL, thread_function, (void *)thread_id);
					thread_created[i] = 1;
					char thread_nb[20] = "";
					sprintf(thread_nb, "%d", i);
					write(FD_REPONSE, thread_nb, strlen(thread_nb));
					printf("\t\tRéponse du tube %s ---> Thread n°%s \n", TUBE_REPONSE,thread_nb);
					break;
				    }
			    }
			}


	unlink(TUBE_REPONSE); // Fermeture du tube de demande
	close(FD_REPONSE);     

        write(FD_DEMANDE, "", 0);
    } 

    //sleep(2);

    if (Crucifix == 1) {
        printf("Serveur :\tBreak Main annoncé\n");
        break;
    }
}

 closedir(dir);

    printf("Serveur :\tAttente de la fin des threads\n");

    for (int i = 0; i < max_thread; i++) {
        if (thread_created[i] ==1 ) {
            pthread_join(threads[i], NULL);
        }
    }

    printf("Serveur :\tTous les threads sont terminés.\n");

    unlink(TUBE_DEMANDE);
    close(FD_DEMANDE);

    return 0;
}
