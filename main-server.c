#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "main-server.h"

int main(int argc, char *argv[])
{
	system("clear");
	printf("--------------------------------------------------------------------------------");
	printf("                                SERVEUR CENTRAL\n");
	printf("--------------------------------------------------------------------------------\n");
	
	int status;
	int sock_err;
	
	// Socket et contexte d'adressage du serveur.
	SOCKET sock;
	SOCKADDR_IN sin;
	socklen_t ssize = sizeof(sin);
	
	// Socket et contexte d'adressage du client.
	SOCKET csock;
	SOCKADDR_IN csin;
	socklen_t csize = sizeof(csin);
	
	sock = socket(AF_INET, SOCK_STREAM, 0);
	
	if(sock == INVALID_SOCKET)
	{
		printf("Erreur : echec de la creation de la socket.\n");
		return EXIT_FAILURE;
	}
	
	// Paramétrage de la socket.
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(SERVER_PORT);
	
	sock_err = bind(sock, (SOCKADDR*)&sin, ssize);
	
	if(sock_err == SOCKET_ERROR)
	{
		printf("Erreur : echec du parametrage de la socket.\n");
		return EXIT_FAILURE;
	}
	
	// 5 est le nombre maximal de connexions pouvant être mises en attente.
	sock_err = listen(sock, 5);
	
	if(sock_err == SOCKET_ERROR)
	{
		printf("Erreur : echec de l'ecoute de la socket.\n");
		return EXIT_FAILURE;
	}
	
	/*
	 * Le serveur central tourne indéfiniment à l'écoute de nouvelles connexions.
	 * A chaque connexion, un processus est créé pour gérer le client à l'aide de la fonction handle_client().
	 */
	while(1)
	{
		csock = accept(sock, (SOCKADDR*)&csin, &csize);
		printf("Connexion de %s sur la socket %d depuis le port %d.\n\n", inet_ntoa(csin.sin_addr),
				csock, htons(csin.sin_port));
		
		if(csock == INVALID_SOCKET)
		{
			printf("Erreur : echec de la connexion avec le client.\n");
			return EXIT_FAILURE;
		}
		
		switch(fork())
		{
			case -1 :	// Erreur.
			break;
			
			case 0 :	// Fils.
				handle_client(csock, csin);
				close(csock);
				kill(getpid(), SIGTERM);
			
			default :	// Père.
				/* WNOHANG est utilisé pour que le père attende la fin
				 * du fils de manière non bloquante.
				 * Lors de l'appel à waitpid(), si le fils est terminé,
				 * on le tue, sinon, on inore sa terminaison. Voilà
				 * pourquoi on se retrouve malheureusement avec un
				 * nombre important de fils zombies qui subsistent
				 * jusqu'à la mort du père. */
				waitpid(-1, &status, WNOHANG);
		}
	}
	
	close(sock);
	
	return EXIT_SUCCESS;
}

/**
 * Gère un nouveau client.
 * 
 * Paramètres :
 * - sock	: la socket sur laquelle communiquer avec le client.
 * - sin	: le contexte d'adressage de cette socket.
 * 
 * Retour : 0 si succès, -1 sinon.
 **/
int handle_client(SOCKET sock, SOCKADDR_IN sin)
{
	char buf[BUFFER_SIZE];
	int sock_err;
	int i;
	char **search_res= (char**)malloc(30*sizeof(char*));
	for(i=0;i<30;i++){
		search_res[i] = (char*) malloc (50*sizeof(char));
	}
	int nbr_files =0;
	
	
	
	sock_err = recv(sock, buf, BUFFER_SIZE, 0);
	if(sock_err == SOCKET_ERROR) return -1;
	
	// Tant que le client est connecté, on le gère.
	while(strcmp(buf, "disconnect") != 0)
	{
		// Si le client demande une authentification.
		if(strcmp(buf, "auth") == 0)
		{
			sock_err = -1;
			
			// On initialise buf à authretry simplement pour entrer dans la boucle.
			strcpy(buf, "authretry");
			
			while((sock_err != 0) && (strcmp(buf, "authretry") == 0))
			{
				/*
				 * Si le client abandonne l'authentification, on sort de la boucle.
				 * Cela évite la boucle infinie dans le cas où le client aurait demandé par erreur une
				 * authentification sans avoir de compte sur le réseau.
				 */
				sock_err = check_client(sock, sin);
				sock_err = recv(sock, buf, BUFFER_SIZE, 0);
			}
		}
		
		// Si le client demande la création d'un compte.
		else if(strcmp(buf, "add") == 0)
		{
			sock_err = -1;
			while(sock_err != 0) sock_err = add_client(sock, sin);
		}
		
		// Si le client uploade la liste de ses fichiers partagés.
		else if(strcmp(buf, "uploadfilelist") == 0)
		{
			sock_err = receive_shared_files(sock, sin);
			if(sock_err == -1) return -1;
		}
		
		else if(strcmp(buf, "initok") == 0)
		{
			// On se contente de réceptionner cet ACK. Aucun traitement n'est nécessaire.
		}
		
		// Si le client demande le téléchargement d'un fichier.
		else if(strcmp(buf, "download") == 0)
		{
			sock_err = get_owner(sock, sin);
			if(sock_err == -1) return -1;
		}
		//si le client demande une recherche par mots clés
		else if(strcmp(buf,"search") == 0)
		{
			
			nbr_files = search(sock,search_res);
			send_search_result(sock,nbr_files,search_res);
			
		}
		
		sock_err = recv(sock, buf, BUFFER_SIZE, 0);
		if(sock_err == SOCKET_ERROR) return -1;
		
		/* Gestion de l'arrêt brutal d'un client.
		 * Si le client se déconnecte sans fermer la socket, recv retourne 0.
		 * Si on ne gère pas ce cas, le serveur tourne en boucle avec le dernier envoi du client
		 * et ne répond plus. */
		if(sock_err == 0)
		{
			printf("Arret critique du client.\n");
			disconnect_client(sin);
			return -1;
		}
	}
	
	disconnect_client(sin);
	
	return 0;
}


/**
 * Vérifie l'identité d'un client du réseau à partir de la base de données.
 * 
 * Paramètre :
 * - sock	: la socket sur laquelle reçevoir les informations d'authentification.
 * - sin	: le contexte d'adressage de cette socket.
 * 
 * Retour : 0 si succès, -1 sinon.
 **/
int check_client(SOCKET sock, SOCKADDR_IN sin)
{
	int sock_err;
	char *query;
	sqlite3_stmt *stmt;
	char password_db[20];	// Le mot de passe contenu dans la base.
	
	char buf[BUFFER_SIZE];
	char login[BUFFER_SIZE];
	char password[BUFFER_SIZE];
	
	// Pointeur sur la base de données du serveur.
	sqlite3 *db;
	int db_err;
	
	db_err = sqlite3_open("main-server.db", &db);
	
	if(db_err)
	{
		printf("Erreur : echec de l'ouverture de la base de donnees.\n");
		return EXIT_FAILURE;
	}
	
	sock_err = recv(sock, login, BUFFER_SIZE, 0);
	
	if(sock_err == SOCKET_ERROR) return -1;
	
	sock_err = recv(sock, password, BUFFER_SIZE, 0);
	
	if(sock_err == SOCKET_ERROR) return -1;
	
	printf("Un client demande une authentification :\n");
	printf("- identifiant : %s\n", login);
	printf("- mot de passe : %s\n\n", password);
	
	// Récupération du mot de passe associé à l'utilisateur et comparaison avec celui transmis par le client.
	query = sqlite3_mprintf("SELECT mdp FROM Utilisateurs WHERE id = '%q';", login);
	
	sqlite3_prepare(db, query, -1, &stmt, NULL);
	
	sock_err = sqlite3_step(stmt);	// Exécution de la requête.
	
	// Si le nom d'utilisateur est incorrect.
	if((char*)sqlite3_column_text(stmt, 0) == NULL)
	{
		strcpy(buf, "authko");
		sock_err = send(sock, buf, BUFFER_SIZE, 0);
		printf("Echec de l'authentification.\n");
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return -1;
	}
	
	strcpy(password_db, (char*)sqlite3_column_text(stmt, 0 ));	// Lecture du résultat.
	
	// Si le mot de passe est correct.
	if((strcmp(password_db, password) == 0))
    {
		strcpy(buf, "authok");
		sock_err = send(sock, buf, BUFFER_SIZE, 0);
		printf("Authentification effectuee.\n");
		
		// Mise à jour de l'adresse IP de l'utilisateur.
		query = sqlite3_mprintf("UPDATE Utilisateurs SET ip = '%q' WHERE id = '%q';",
							inet_ntoa(sin.sin_addr), login);
		
		sqlite3_exec(db, query, NULL, 0, NULL);
		
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		
		return 0;
	}
	
	else
	{
		strcpy(buf, "authko");
		sock_err = send(sock, buf, BUFFER_SIZE, 0);
		printf("Echec de l'authentification.\n");
		
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		
		return -1;
	}
}

/**
 * Ajoute un client à la base de données.
 * 
 * Paramètres :
 * - sock	: la socket sur laquelle reçevoir les informations d'authentification.
 * - sin	: le contexte d'adressage de cette socket.
 * 
 * Retour : 0 si succès, -1 sinon.
 **/
int add_client(SOCKET sock, SOCKADDR_IN sin)
{
	int db_err;
	int sock_err;
	char *zErrMsg = 0;
	char *query;
	char buf[BUFFER_SIZE];
	char login[BUFFER_SIZE];
	char password[BUFFER_SIZE];
	
	// Pointeur sur la base de données du serveur.
	sqlite3 *db;
	
	db_err = sqlite3_open("main-server.db", &db);
	
	if(db_err)
	{
		printf("Erreur : echec de l'ouverture de la base de donnees.\n");
		return EXIT_FAILURE;
	}
	
	sock_err = recv(sock, login, BUFFER_SIZE, 0);
	
	if(sock_err == SOCKET_ERROR) return -1;
	
	sock_err = recv(sock, password, BUFFER_SIZE, 0);
	
	if(sock_err == SOCKET_ERROR) return -1;
	
	// Ajout du nouveau client à la base.
	
	// Préparation de la requête.
	query = sqlite3_mprintf("INSERT INTO Utilisateurs VALUES('%q', '%q', '%q');",
							login, password, inet_ntoa(sin.sin_addr));
	
	// Exécution de la requête.
	db_err = sqlite3_exec(db, query, NULL, 0, &zErrMsg);
	
	// L'identifiant existe déjà dans la base. On prévient le client.
	if(db_err != SQLITE_OK)
	{
		printf("Erreur SQL : %s\n", zErrMsg);
		
		strcpy(buf, "addko");
		sock_err = send(sock, buf, BUFFER_SIZE, 0);
		
		if(sock_err == SOCKET_ERROR) return -1;
		
		sqlite3_close(db);
		
		return -1;
	}
	
	else
	{
		// On confirme au client que l'ajout à la base s'est bien déroulé.
		strcpy(buf, "addok");
		sock_err = send(sock, buf, BUFFER_SIZE, 0);
		
		if(sock_err == SOCKET_ERROR) return -1;
	}
	
	printf("Un client cree un compte.\n\n");
	
	sqlite3_close(db);
	
	return 0;
}

/**
 * Insère les fichiers du client nouvellement connecté à la base de données.
 * 
 * Paramètres :
 * - sock	: la socket sur laquelle est connecté le client
 * - sin	: le contexte d'adressage de cette socket.
 * 
 * Retour : 0 si succès, -1 sinon.
 **/
int receive_shared_files(SOCKET sock, SOCKADDR_IN sin)
{
	int sock_err, db_err;
	char *query, *zErrMsg;
	sqlite3_stmt *stmt;
	char path[BUFFER_SIZE];
	char description[BUFFER_SIZE];
	char owner[16];
	
	// Pointeur sur la base de données du serveur.
	sqlite3 *db;
	
	db_err = sqlite3_open("main-server.db", &db);
	
	if(db_err)
	{
		printf("Erreur : echec de l'ouverture de la base de donnees.\n");
		return EXIT_FAILURE;
	}
	
	printf("Un client uploade ses fichiers.\n");
	
	// Récupération de l'identifiant du propriétaire du fichier.
	query = sqlite3_mprintf("SELECT id FROM Utilisateurs WHERE ip = '%q';", inet_ntoa(sin.sin_addr));
	sqlite3_prepare(db, query, -1, &stmt, NULL);
	sock_err = sqlite3_step(stmt);	// Exécution de la requête.
	
	// Si aucun identifiant n'a été récupéré (ne devrait jamais arriver).
	if((char*)sqlite3_column_text(stmt, 0) == NULL)
	{
		printf("Erreur : utilisateur inconnu.\n");
		sqlite3_close(db);
		return -1;
	}
	
	strcpy(owner, (char*)sqlite3_column_text(stmt, 0));
	sqlite3_finalize(stmt);
	
	sock_err = recv(sock, path, BUFFER_SIZE, 0);
	if(sock_err == SOCKET_ERROR) return -1;
	
	// Tant que le client n'a pas envoyé tous les fichiers.
	while(strcmp(path, "endoffilelist") != 0)
	{
		/* La réception d'un tuple se fait en deux temps :
		 * 1. Chemin du fichier.
		 * 2. Description du fichier. */
		
		// Réception de la description (le chemin a déjà été lu).
		sock_err = recv(sock, description, BUFFER_SIZE, 0);
		if(sock_err == SOCKET_ERROR) return -1;
		
		/* ATTENTION !
		 * PAS BESOIN DE L'ID ! Si deux fois le même fichier dans la base, peu importe.
		 * Si un veut télécharger fic.txt et que deux utilisateurs le proposent, on prend le premier qui vient.
		 * Trouver le propriétaire grâce à l'IP dans la base.
		 * PAS BESOIN DE LA TAILLE ! ON LA PASSERA JUSTE LORSQUE QUELQU'UN EN AURA BESOIN EN LA CALCULANT DIRECTEMENT
		 * SUR LE FICHIER ! */	
		
		query = sqlite3_mprintf("INSERT INTO Fichiers VALUES('%q', '%q', '%q');",
								path, owner, description);
		db_err = sqlite3_exec(db, query, NULL, 0, &zErrMsg);
		
		if(db_err != SQLITE_OK)
		{
			printf("Erreur SQL : %s\n", zErrMsg);
			sqlite3_close(db);
			return -1;
		}
		
		// Réception du tuple suivant.
		sock_err = recv(sock, path, BUFFER_SIZE, 0);
		if(sock_err == SOCKET_ERROR) return -1;
	}
	
	sqlite3_close(db);
	
	return 0;
}

/**
 * Supprime les fichiers partagés d'un client qui se déconnecte de la base de données.
 * 
 * Paramètres :
 * - login	: le login de ce client.
 * 
 * Retour : 0 si succès, -1 sinon.
 * 
 **/
int disconnect_client(SOCKADDR_IN sin)
{
	int db_err;
	char *query, *zErrMsg;
	
	// Pointeur sur la base de données du serveur.
	sqlite3 *db;
	
	db_err = sqlite3_open("main-server.db", &db);
	
	if(db_err)
	{
		printf("Erreur : echec de l'ouverture de la base de donnees.\n");
		return EXIT_FAILURE;
	}
	
	// Préparation de la requête.
	query = sqlite3_mprintf("DELETE FROM Fichiers \
							WHERE proprietaire \
							IN(SELECT id FROM Utilisateurs WHERE ip = '%q');", inet_ntoa(sin.sin_addr));
	
	// Exécution de la requête.
	db_err = sqlite3_exec(db, query, NULL, 0, &zErrMsg);
	
	if(db_err != SQLITE_OK)
	{
		printf("Erreur SQL : %s\n", zErrMsg);
		sqlite3_close(db);
		return -1;
	}
	
	printf("Deconnexion de %s.\n\n", inet_ntoa(sin.sin_addr));
	
	sqlite3_close(db);
	
	return 0;
}

/**
 * Transmet l'IP du propriétaire d'un fichier demandé par un client.
 * 
 * Paramètres :
 * - sock	: la socket sur laquelle est connecté le client
 * - sin	: le contexte d'adressage de cette socket.
 * 
 * Retour : 0 si succès, -1 sinon.
 **/
int get_owner(SOCKET sock, SOCKADDR_IN sin)
{
	char buf[BUFFER_SIZE];
	int sock_err;
	char *query;
	sqlite3_stmt *stmt;
	
	// Pointeur sur la base de données du serveur.
	sqlite3 *db;
	int db_err;
	
	db_err = sqlite3_open("main-server.db", &db);
	
	if(db_err)
	{
		printf("Erreur : echec de l'ouverture de la base de donnees.\n");
		return EXIT_FAILURE;
	}
	
	// Réception du nom du fichier.
	sock_err = recv(sock, buf, BUFFER_SIZE, 0);
	if(sock_err == SOCKET_ERROR) return -1;
	
	printf("Un client demande le fichier \"%s\".\n\n", buf);
	
	// Recherche de l'IP du propriétaire dans la base.
	query = sqlite3_mprintf("SELECT Utilisateurs.ip FROM Utilisateurs, Fichiers \
							WHERE Utilisateurs.id = Fichiers.proprietaire \
							AND Fichiers.chemin = '%q';", buf);
	sqlite3_prepare(db, query, -1, &stmt, NULL);
	sock_err = sqlite3_step(stmt);	// Exécution de la requête.
	
	// Envoi de l'IP du propriétaire du fichier.
	if((char*)sqlite3_column_text(stmt, 0) == NULL)
	{
		strcpy(buf, "unknownfile");
		sock_err = send(sock, buf, BUFFER_SIZE, 0);
		if(sock_err == SOCKET_ERROR) return -1;
	}
	
	else
	{
		strcpy(buf, (char*)sqlite3_column_text(stmt, 0));
		sock_err = send(sock, buf, BUFFER_SIZE, 0);
		if(sock_err == SOCKET_ERROR) return -1;
	}
	
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	
	return 0;
}
/**
 * Fonction pour recevoir les mots clés à chercher dans la database
 * 
 * Paramètre :
 * - sock	: la socket sur laquelle reçevoir les informations d'authentification.
 * 
 * Retour : le nombre de fichiers trouvés.
 * */
int search(SOCKET sock, char** search_res)
{
	char searchbuf[BUFFER_SIZE];
	int nbr=0;
	int i,k,sock_err;
	char * comp_desc = NULL;
	char * comp_name = NULL;
	int j=0;
		
	sqlite3 *db;
	int db_err;
	char *query;
	sqlite3_stmt *stmt;	
	char buf[BUFFER_SIZE];
	
	//Recevoir le nombre de mots à chercher
	sock_err = recv(sock, &nbr, BUFFER_SIZE, 0);
	
	if(sock_err == SOCKET_ERROR) return -1;
	//printf("le nombre de mots reçu est = %d\n",nbr);	
		
	if(nbr != 0) {
		//recevoir la liste des mots clés à chercher
		printf("Reception des mots clés à chercher ...\n");
		
		for (i=0;i<nbr;i++){
			sock_err = recv(sock, searchbuf, BUFFER_SIZE, 0);
		
			if(sock_err == SOCKET_ERROR) return -1;
			else
			{
				printf("Recu: searchbuf[%d] = %s\n",i,searchbuf);
			
				//recupere tous les fichiers de main-server.db
				db_err = sqlite3_open("main-server.db", &db);
				
				if(db_err)
				{
					printf("Erreur : echec de l'ouverture de la base de donnees.\n");
					return EXIT_FAILURE;
				}
				// Rechercher si y a des fichiers dans la base qui contiennent un de ces mots clés.
				query = sqlite3_mprintf("SELECT * FROM Fichiers;"); // on recupere tous les fichiers de la base
						
				sqlite3_prepare(db, query, -1, &stmt, NULL);
				db_err = sqlite3_step(stmt);
				
				// Tant qu'il reste des tuples à lire.
				while(db_err == SQLITE_ROW)
				{
					strcpy(buf, (char*)sqlite3_column_text(stmt, 2));	// Lecture de la description du fichier.
				
					comp_desc = strstr(buf,searchbuf);			//comparer avec tous les mots clés dans searchbuf
					
					
					strcpy(buf, (char*)sqlite3_column_text(stmt, 0));	// Lecture du nom du fichier. 
					comp_name = strstr(buf,searchbuf);			//comparer avec tous les mots clés				
					
					
					//si la description ou le nom contient le mot clé => envoie à l'utilisateur
					if(comp_name != NULL || comp_desc != NULL) 
					{
						printf("Le mot  %s est trouvé dans %s\n", searchbuf,buf);
						 int exist = 0;
						//ajouter le nom du fichier trouvé au tableau des resultats s'il n'existe pas déjà
						for(k =0; k<j;k++){
							if(strcmp(search_res[k],buf) == 0)
								exist = 1;
						}
						if(exist == 0){
							strcpy(search_res[j],buf);
							j++;
						}
						
					}
					
					db_err = sqlite3_step(stmt); //avancer au prochain fichier
				}
				
				sqlite3_close(db);
			}
		}
	}
	//envoyer le nombre de fichiers trouvé au terminal
	sock_err=send(sock,&j,BUFFER_SIZE,0);
	if(sock_err == SOCKET_ERROR) return -1;
	
return j;
}		
				
int send_search_result(SOCKET sock, int nbr_files,char ** search_res){
	
	int i,sock_err;
	
	for(i= 0; i<nbr_files;i++){
		sock_err = send(sock,search_res[i], BUFFER_SIZE, 0);
			if(sock_err == SOCKET_ERROR) return -1;
			else {
				printf("Le fichier %s est envoyé au terminal.\n",search_res[i]);}
	}
	return 0;
}
			
