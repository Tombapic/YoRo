#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sqlite3.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "server.h"
#include "client.h"

int main(int argc, char *argv[])
{
	if(argc != 2)
	{
		printf("Utilisation : host <ip_du_serveur>\n");
		return EXIT_FAILURE;
	}
	
	print_header();
	
	// Pointeur sur la base de données du serveur.
	sqlite3 *db;
	int db_err;
	
	int status;
	int sock_err;
	int user_choice;
	char *server_addr = argv[1];
	
	db_err = sqlite3_open("server.db", &db);
	
	if(db_err)
	{
		printf("Erreur : echec de l'ouverture de la base de donnees.\n");
		return EXIT_FAILURE;
	}
	
	// Socket et contexte d'adressage du serveur.
	SOCKET sock;
	SOCKADDR_IN sin;
	socklen_t ssize = sizeof(sin);
	
	// Socket et contexte d'adressage du serveur central.
	SOCKET main_sock;
	SOCKADDR_IN main_sin;
	socklen_t main_size = sizeof(main_sin);
	
	// Socket et contexte d'adressage du pair.
	SOCKET csock;
	SOCKADDR_IN csin;
	socklen_t csize = sizeof(csin);
	
	sock = socket(AF_INET, SOCK_STREAM, 0);
	main_sock = socket(AF_INET, SOCK_STREAM, 0);
	
	if(sock == INVALID_SOCKET)
	{
		printf("Erreur : echec de la creation de la socket.\n");
		return EXIT_FAILURE;
	}
	
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(HOST_PORT);
	
	main_sin.sin_addr.s_addr = inet_addr(server_addr);
	main_sin.sin_family = AF_INET;
	main_sin.sin_port = htons(SERVER_PORT);
	
	// En premier lieu, connexion au serveur central et authentification.
	sock_err = connect(main_sock, (SOCKADDR*)&main_sin, main_size);
	
	if(sock_err == SOCKET_ERROR)
	{
		printf("Erreur : echec de la connexion au serveur central.\n");
		return EXIT_FAILURE;
	}
	
	printf("Connexion à %s sur le port %d\n", inet_ntoa(main_sin.sin_addr), htons(main_sin.sin_port));
	
	printf("1. S'authentifier\n");
	printf("2. Creer un compte\n");
	scanf("%1d", &user_choice);
	
	while((user_choice!=1) && (user_choice!=2)) scanf("%1d", &user_choice);
	
	if(user_choice == 1)
	{
		sock_err = authenticate(main_sock);
		
		// Si erreur ou abandon de l'authentification par l'utilisateur.
		if((sock_err==-1) || (sock_err==1))
		{
			printf("Erreur : echec de l'authentification auprès du serveur.\n");
			return EXIT_FAILURE;
		}
	}
	
	else
	{
		sock_err = create_account(main_sock);
		
		if(sock_err == -1)
		{
			printf("Erreur : echec de la creation du compte.\n");
			return EXIT_FAILURE;
		}
	}
	
	/*
	 * Maintenant que le serveur local est lancé, on met à jour la table des fichiers partagés
	 * puis, dans un second temps, on envoie la liste de ces fichiers au serveur central.
	 */
	update_shared_files();
	
	sock_err = send_shared_files(main_sock);
	if(sock_err == -1)
	{
		printf("Erreur : echec de l'envoi de la liste des fichiers partages.\n");
		return EXIT_FAILURE;
	}
	
	/* Maintenant que le serveur local est initialisé, on rentre dans
	 * la boucle d'écoute qui permet de traiter d'éventuels clients. */
	sock_err = send(main_sock, "initok", BUFFER_SIZE, 0);
	
	FILE *file = NULL;
	
	/* On crée un processus chargé d'écouter les nouveaux clients.
	 * Le processus père sera chargé d'exécuter les demandes de
	 * l'utilisateur. */
	switch(fork())
		{
			case -1 :	// Erreur.
			break;
			
			case 0 :	// Fils.
				file = fopen("log.txt", "w");
				fputs("fork\n", file);
				fclose(file);
				
				while(1)
				{
					sock_err = bind(sock, (SOCKADDR*)&sin, ssize);
	
					// 5 est le nombre maximal de connexions pouvant être mises en attente.
					sock_err = listen(sock, 5);
					
					if(sock_err == SOCKET_ERROR)
					{
						printf("Erreur : echec de l'ecoute de la socket.\n");
						return EXIT_FAILURE;
					}
					
					csock = accept(sock, (SOCKADDR*)&csin, &csize);
					
					if(csock == INVALID_SOCKET)
						return EXIT_FAILURE;
					
					file = fopen("log.txt", "a");
					fputs("accept\n", file);
					fclose(file);
					
					switch(fork())
					{
						case -1 :	// Erreur.
						break;
						
						case 0 :	// Fils.
							handle_peer(csock, csin, db);
							close(csock);
							kill(getpid(), SIGTERM);
						
						default :	// Père.
							waitpid(-1, &status, WNOHANG);
							kill(getpid(), SIGTERM);
					}
				}
			
			default :	// Père.
				// Le terminal attend les instructions de l'utilisateur.
				handle_user(main_sock, main_sin);
				
				// Déconnexion de l'utilisateur.
				sock_err = send(main_sock, "disconnect", BUFFER_SIZE, 0);
				
				if(sock_err == SOCKET_ERROR)
					return EXIT_FAILURE;
				
				waitpid(-1, &status, WNOHANG);
		}
	
	close(sock);
	
	return EXIT_SUCCESS;
}
