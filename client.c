#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include "client.h"

/**
 * Récupère les commandes de l'utilisateur et exécute les fonctions correspondantes.
 * 
 * Paramètres :
 * - sock	: la socket sur laquelle communiquer avec le serveur cental.
 * - sin	: le contexte d'adressage de cette socket.
 * 
 * Retour : 0 si succès, -1 sinon.
 **/
int handle_user(SOCKET sock, SOCKADDR_IN sin)
{
	int user_choice = 0;
	int nbr_files =0;
	print_header();
	
	// Tant que l'utilisateur est connecté.
	while(user_choice != 3)
	{
			print_header();
			printf("1. Rechercher un fichier\n");
			printf("2. Telecharger un fichier\n");
			printf("3. Se deconnecter\n");
			
			scanf("%1d", &user_choice);
			
			if(user_choice == 1) {
				nbr_files = search_request(sock);
				receive_search_result(sock,nbr_files);
				}
				// Appel de la fonction de recherche de fichier codée par Yossra.
			
			else if(user_choice == 2)
				download_file(sock);
	}
	
	return 0;
}

/**
 * Gère un nouvel hôte distant.
 * 
 * Paramètres :
 * - sock	: la socket sur laquelle communiquer avec l'hôte.
 * - sin	: le contexte d'adressage de cette socket.
 * 
 * Retour : 0 si succès, -1 sinon.
 **/
int handle_peer(SOCKET sock, SOCKADDR_IN sin)
{
	FILE *file = NULL;
	char buf[BUFFER_SIZE];
	char path[100] = "P2P/";
	struct stat st;
	int fd, sock_err, remaining_bytes;
	
	// Réception du nom du fichier à télécharger.
	sock_err = recv(sock, buf, BUFFER_SIZE, 0);
	if(sock_err == SOCKET_ERROR) return -1;
	
	strcat(path, buf);
	
	fd = open(path, O_RDONLY);
	fstat(fd, &st);
	close(fd);
	
	// Convertit un entier en chaîne de caractère.
	sprintf(buf, "%d", (int)st.st_size);
	
	// Envoi de la taille du fichier.
	sock_err = send(sock, buf, BUFFER_SIZE, 0);
	if(sock_err == SOCKET_ERROR) return -1;
	
	file = fopen(path, "r");
	
	remaining_bytes = st.st_size;
	
	// Envoi du fichier par paquets de 512 octets.
	while(remaining_bytes > 0)
	{
		if(remaining_bytes >= BUFFER_SIZE)
		{
			fread(buf, BUFFER_SIZE, 1, file);
			remaining_bytes = remaining_bytes - BUFFER_SIZE;
		}
		
		else
		{
			fread(buf, remaining_bytes, 1, file);
			remaining_bytes = 0;
		}
		
		sock_err = send(sock, buf, BUFFER_SIZE, 0);
		if(sock_err == SOCKET_ERROR) return -1;
	}
	
	fclose(file);
	
	return 0;
}

/**
 * Télécharge un fichier chez un hôte distant.
 * 
 * Paramètres :
 * - sock	: la socket sur laquelle communiquer avec le serveur central.
 * 
 * Retour : 0 si succès, -1 sinon.
 **/
int download_file(SOCKET sock)
{
	char buf[BUFFER_SIZE];
	int sock_err;
	
	FILE* file = NULL;
	
	char file_name[100];
	// Préfixe ajouté au nom du fichier pour le placer dans le répertoire des fichiers partagés.
	char path[100] = "P2P/dl_";
	int file_size, remaining_bytes;
	
	// Socket et contexte d'adressage de l'hôte.
	SOCKET hsock;
	SOCKADDR_IN hsin;
	socklen_t hsize = sizeof(hsin);
	
	print_header();
	
	printf("Nom du fichier à telecharger ?\n\n");
	// On réserve les derniers caractères pour le caractère de fin de chaîne et le préfixe "P2P/".
	scanf("%95s", file_name);
	
	// On indique au serveur que l'on désire télécharger un fichier.
	strcpy(buf, "download");
	sock_err = send(sock, buf, BUFFER_SIZE, 0);
	if(sock_err == SOCKET_ERROR) return -1;
	
	// On indique au serveur le nom du fichier à télécharger.
	strcpy(buf, file_name);
	sock_err = send(sock, buf, BUFFER_SIZE, 0);
	if(sock_err == SOCKET_ERROR) return -1;
	
	// Le serveur envoie l'IP du propriétaire du fichier ou "unknownfile" si le fichier n'existe pas.
	sock_err = recv(sock, buf, BUFFER_SIZE, 0);
	if(sock_err == SOCKET_ERROR) return -1;
	
	if(strcmp(buf, "unknownfile") == 0)
	{
		printf("Erreur : le fichier n'existe pas sur le réseau.\n\n");
		return -1;
	}
	
	// On se branche sur cet hôte.
	hsock = socket(AF_INET, SOCK_STREAM, 0);
	hsin.sin_addr.s_addr = inet_addr(buf);
	hsin.sin_family = AF_INET;
	hsin.sin_port = htons(HOST_PORT);
	
	sock_err = connect(hsock, (SOCKADDR*)&hsin, hsize);
	
	if(sock_err == SOCKET_ERROR)
	{
		printf("Erreur : echec de la connexion a l'hote.\n");
		return EXIT_FAILURE;
	}
	
	// On indique à l'hôte le nom du fichier à télécharger.
	strcpy(buf, file_name);
	sock_err = send(hsock, buf, BUFFER_SIZE, 0);
	if(sock_err == SOCKET_ERROR) return -1;
	printf("fichier : %s\n\n", buf);
	
	// L'hôte envoie la taille du fichier.
	sock_err = recv(hsock, buf, BUFFER_SIZE, 0);
	if(sock_err == SOCKET_ERROR) return -1;
	
	file_size = atoi(buf);
	
	printf("taille du fichier : %d\n\n", file_size);
	
	/* On crée un nouveau fichier et on le construit au fur et à mesure
	 * de la réception des octets. */
	strcat(path, file_name);
	file = fopen(path, "w");
	
	remaining_bytes = file_size;
	
	while(remaining_bytes > 0)
	{
		// Ces affichages ralentissent énormément le programme, mais pour la soutenance, c'est la classe...
		print_header();
		
		// Pour afficher "%", on doit écrire "%%".
		printf("Telechargement %d%%\n\n", 100 - (int)(((float)remaining_bytes/(float)file_size)*100));
		
		sock_err = recv(hsock, buf, BUFFER_SIZE, 0);
		if(sock_err == SOCKET_ERROR) return -1;
		
		if(remaining_bytes >= BUFFER_SIZE)
		{
			fwrite(buf, BUFFER_SIZE, 1, file);
			remaining_bytes = remaining_bytes - BUFFER_SIZE;
		}
		
		else
		{
			fwrite(buf, remaining_bytes, 1, file);
			remaining_bytes = 0;
		}
	}
	
	fclose(file);
	
	return 0;
}

/**
 * Affiche l'entête du programme.
 * 
 **/
void print_header()
{
	system("clear");
	printf("--------------------------------------------------------------------------------");
	printf("                                      HOTE\n");
	printf("--------------------------------------------------------------------------------\n");
}
/**
 * si l'utilisateur veut faire une recherche par mots clé, cette fonction prévient le serveur central 
 * et lui envoie la liste des mots a chercher.
 * 
 * Paramètre :
 * - sock : la socket sur laquelle envoyer les informations d'authentification.
 * 
 * Retour : retourne le nombre de fichiers trouvés.
 **/
int search_request(SOCKET sock){
	
	int sock_err=0;
	char buf[BUFFER_SIZE];
	int i; 
	int nbr_mots;
	int nbr_files=0;
	char text[BUFFER_SIZE];
		
	char **tableau= (char**)malloc(30*sizeof(char*));
	for(i=0;i<30;i++){
		tableau[i] = (char*) malloc (50*sizeof(char));
	}
	/*
	 * for(i=0;i<10;++i) free(tableau[i]);
		free(tableau);
	 */
	
		//prevenir le serveur central
		strcpy(buf, "search"); 
		sock_err = send(sock, buf, BUFFER_SIZE, 0);
		
		if(sock_err == SOCKET_ERROR) { 
			printf("sock_err (send search)= %d\n",sock_err);
			return -1;
		}
	
		/*Demander à l'utilisateur la liste de mots clés à chercher */
				printf("Appuyez sur une touche Puis donnez la liste de vos mots clé \n");
				getchar();
				getchar();
				fgets(text,sizeof text,stdin);
				char *p = strchr(text, '\n');
					if (p)
					{
						*p = 0; 
					}

				/* La fonction cut nous permet de récupérer les mots clés dans un tableau*/
				nbr_mots =cut(tableau,text);
				
			
				
					// Envoyer le nombre de mots à chercher
					sock_err = send(sock,&nbr_mots, BUFFER_SIZE, 0);
					if(sock_err == SOCKET_ERROR) return -1;

					/* Il faut envoyer le tableau de mots au serveur pour qu'il effectue la recherche*/	
					for(i=0; i<nbr_mots;i++){
						sock_err = send(sock, tableau[i], BUFFER_SIZE, 0);	
						
						if(sock_err == SOCKET_ERROR) return -1;	
					}
					
					/*Ecouter la reponse du serveur central pour recevoir le nombre de fichiers trouvés*/
						sock_err = recv(sock,&nbr_files, BUFFER_SIZE, 0);
		
						if(sock_err == SOCKET_ERROR) return -1;
						else
						{
							printf("\n Le nombre de fichiers trouvés est = %d. \n Appuyez sur une touche pour les afficher.\n" , nbr_files);
							getchar();
								
						}
					
				
	return nbr_files;
}
int cut(char** tableau,char* source){

	char delims[] = " ";
	char *result = NULL;
	int i=0;
	int nombre_mots=0;
	result = strtok( source, delims );
	
		while( result != NULL ) {
			strcpy(tableau[i],result);
			result = strtok( NULL, delims );
			i++;
			nombre_mots= nombre_mots+1;
		}
			
	return nombre_mots;
}
int receive_search_result(SOCKET sock,int nbr_files){
	int sock_err =0;
	int i;
	char buf[BUFFER_SIZE];
	for(i=0; i<nbr_files;i++){
	 	sock_err = recv(sock, buf, BUFFER_SIZE, 0);
		
		if(sock_err == SOCKET_ERROR) return -1;
			else
			{
				printf("%s\n" , buf);
								
			}
	}
	printf("\nAppuyez sur une touche pour retourner au menu principal\n");
	getchar();
	return 0;
}
