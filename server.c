#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>

#include "server.h"

/**
 * Crée un compte sur le réseau.
 * 
 * Paramètre :
 * - sock : la socket sur laquelle envoyer les informations d'authentification.
 * 
 * Retour : 0 si succès, -1 sinon.
 **/
int create_account(SOCKET sock)
{
	int sock_err;
	char buf[BUFFER_SIZE];
	
	// On avertit le serveur central que l'on désire créer un compte.
	strcpy(buf, "add");
	sock_err = send(sock, buf, BUFFER_SIZE, 0);
	
	if(sock_err == SOCKET_ERROR) return -1;
	
	printf("Login ? ");
	scanf("%14s", buf);
	
	sock_err = send(sock, buf, BUFFER_SIZE, 0);
	
	if(sock_err == SOCKET_ERROR) return -1;
	
	printf("Password ? ");
	scanf("%19s", buf);
	
	sock_err = send(sock, buf, BUFFER_SIZE, 0);
	
	if(sock_err == SOCKET_ERROR) return -1;
	
	// Ecoute de la réponse du serveur.
	sock_err = recv(sock, buf, BUFFER_SIZE, 0);
	
	if(sock_err == SOCKET_ERROR) return -1;
	
	// Tant que la création du compte échoue.
	while(strcmp(buf, "addok") != 0)
	{
		printf("Cet identifiant existe deja dans la base, veuillez reessayer.\n");
		
		// Reprise du code précedent.
		printf("Login ? ");
		scanf("%14s", buf);
		
		sock_err = send(sock, buf, BUFFER_SIZE, 0);
		
		if(sock_err == SOCKET_ERROR) return -1;
		
		printf("Password ? ");
		scanf("%19s", buf);
		
		sock_err = send(sock, buf, BUFFER_SIZE, 0);
		
		if(sock_err == SOCKET_ERROR) return -1;
		
		// Ecoute de la réponse du serveur.
		sock_err = recv(sock, buf, BUFFER_SIZE, 0);
		
		if(sock_err == SOCKET_ERROR) return -1;
	}
	
	return 0;
}

/**
 * Demande les authentifiants à l'utilisateur et les envoie au serveur central.
 * 
 * Paramètre :
 * - sock : la socket sur laquelle envoyer les informations d'authentification.
 * 
 * Retour : 0 si succès, 1 si abandon, -1 si erreur.
 **/
int authenticate(SOCKET sock)
{
	int sock_err;
	char user_choice;
	char buf[BUFFER_SIZE];
	
	/*
	 * Annonce de l'authentification au serveur afin qu'il puisse appeler la fonction adéquate de son côté.
	 * Attention ! Quelle que soit la taille de la donnée que l'on envoie, on doit toujours envoyer un paquet complet
	 * de taille BUFFER_SIZE.
	 * En effet, on lit et on envoie paquet par paquet pour assurer la cohérence de la transmission. Il est impossible
	 * d'envoyer des données d'une autre taille.
	 */
	strcpy(buf, "auth");
	sock_err = send(sock, buf, BUFFER_SIZE, 0);
	
	if(sock_err == SOCKET_ERROR) return -1;
	
	printf("Login ? ");
	scanf("%14s", buf);
	
	sock_err = send(sock, buf, BUFFER_SIZE, 0);
	
	if(sock_err == SOCKET_ERROR) return -1;
	
	printf("Password ? ");
	scanf("%19s", buf);
	
	sock_err = send(sock, buf, BUFFER_SIZE, 0);
	
	if(sock_err == SOCKET_ERROR) return -1;
	
	// Ecoute de la réponse du serveur.
	sock_err = recv(sock, buf, BUFFER_SIZE, 0);
	
	// Tant que l'authentification échoue.
	while(strcmp(buf, "authok") != 0)
	{
		printf("Identifiant et/ou mot de passe incorrect. Reessayer ? (o/n) ");
		scanf("%c", &user_choice);
		
		while((user_choice!='o') && (user_choice!='n')) scanf("%c", &user_choice);
		
		if(user_choice == 'o')
		{
			strcpy(buf, "authretry");
			sock_err = send(sock, buf, BUFFER_SIZE, 0);
			
			// Reprise du code précedent.
			printf("Login ? ");
			scanf("%14s", buf);
			
			sock_err = send(sock, buf, BUFFER_SIZE, 0);
			
			if(sock_err == SOCKET_ERROR) return -1;
			
			printf("Password ? ");
			scanf("%19s", buf);
			
			sock_err = send(sock, buf, BUFFER_SIZE, 0);
			
			if(sock_err == SOCKET_ERROR) return -1;
			
			// Ecoute de la réponse du serveur.
			sock_err = recv(sock, buf, BUFFER_SIZE, 0);
			
			if(sock_err == SOCKET_ERROR) return -1;
		}
		
		else
		{
			strcpy(buf, "authabort");
			sock_err = send(sock, buf, BUFFER_SIZE, 0);
	
			if(sock_err == SOCKET_ERROR) return -1;
			
			printf("Authentification annulee.\n");
			printf("Appuyez sur une touche pour continuer.\n");
			getchar();
			getchar();
			
			return 1;
		}
	}
	
	/* Send "de bourrage". Comme on peut envoyer une réponse si l'on veut un nouvel essai
	 * d'authentification, il en faut un aussi, bien qu'inutile, dans le cas où tout se passe bien
	 * sous peine de désynchronisation. */
	strcpy(buf, "authack");
	sock_err = send(sock, buf, BUFFER_SIZE, 0);
	
	if(sock_err == SOCKET_ERROR) return -1;
	
	printf("Vous etes maintenant authentifie.\n");
	printf("Appuyez sur une touche pour continuer.\n");
	getchar();
	getchar();
	
	return 0;
}

/**
 * Lit le répertoire des fichiers partagés et met à jour la base locale de l'utilisateur.
 * 
 * Retour : 0 si succès, -1 sinon.
 **/
int update_shared_files()
{
	sqlite3 *db;
	sqlite3_stmt *stmt;
	char *query, *zErrMsg=0;
	int db_err, found=0;
	DIR *dir;
	struct dirent *dp;
	
	db_err = sqlite3_open("server.db", &db);
	
	if(db_err)
	{
		printf("Erreur : echec de l'ouverture de la base de donnees.\n");
		return EXIT_FAILURE;
	}
	
	/* Avant toute chose, on supprime de la base les fichiers qui ne sont plus dans le répertoire.
	 * Si l'utilisateur supprime un fichier du répertoire des fichiers partagés, ce dernier ne doit
	 * plus apparaître dans la base de données. */
	 
	/* Pour chaque fichier de la base locale, on parcourt le répertoire :
	 * - si le fichier est trouvé dans le répertoire, on le laisse
	 * - sinon, on le supprime de la base.
	 * 
	 * Cette manière de procéder est très coûteuse en accès mémoire, mais simple à programmer ! */
	query = sqlite3_mprintf("SELECT chemin FROM Fichiers;");
	sqlite3_prepare(db, query, -1, &stmt, NULL);
	
	db_err = sqlite3_step(stmt);
	
	// Tant qu'il reste des tuples à lire.
	while(db_err == SQLITE_ROW)
	{
		// Parcours du répertoire.
		dir = opendir("P2P");
		
		if(dir == NULL)
		{
			printf("Erreur : echec de l'ouverture du repertoire des fichiers partages.\n");
			return -1;
		}
		
		dp = readdir(dir);
		
		while(dp != NULL)
		{
			if(strcmp(dp->d_name, (char*)sqlite3_column_text(stmt, 0)) == 0)
				found = 1;
			
			dp = readdir(dir);
		}
		
		// Si le fichier n'a pas été trouvé, on le supprime de la base.
		if(found == 0)
		{
			query = sqlite3_mprintf("DELETE FROM Fichiers WHERE chemin = '%q';", (char*)sqlite3_column_text(stmt, 0));
			db_err = sqlite3_exec(db, query, NULL, 0, NULL);
		}
		
		found = 0;
		closedir(dir);
		db_err = sqlite3_step(stmt);
	}
	
	// On ajoute ensuite les nouveaux fichiers. Ceux déjà présents dans la base ne seront pas modifiés.
	dir = opendir("P2P");
	
	if(dir == NULL)
	{
		printf("Erreur : echec de l'ouverture du repertoire des fichiers partages.\n");
		return -1;
	}
	
	dp = readdir(dir);
	
	while(dp != NULL)
	{
		// On ignore les fichiers cachés.
		if(dp->d_name[0] != '.')
		{
			/* Ajout du nouveau client à la base. Comme le chemin du fichier est la clé primaire,
			 * si le fichier existe déjà, il ne sera pas ajouté. */
			
			// Préparation de la requête. Pour le moment, on n'entre aucune description.
			query = sqlite3_mprintf("INSERT OR IGNORE INTO Fichiers VALUES('%q', '');", dp->d_name);
			
			// Exécution de la requête.
			db_err = sqlite3_exec(db, query, NULL, 0, &zErrMsg);
			
			if(db_err != SQLITE_OK)
			{
				printf("Erreur SQL : %s\n", zErrMsg);
				return -1;
			}
		}
		
		dp = readdir(dir);
	}
	
	closedir(dir);
	
	return 0;
}

/**
 * Envoie la liste des fichiers partagés au serveur central.
 * 
 * Paramètre :
 * - sock : la socket connectée au serveur central.
 * 
 * Retour : 0 si succès, -1 sinon.
 **/
int send_shared_files(SOCKET sock)
{
	int sock_err, db_err;
	sqlite3 *db;
	char *query;
	sqlite3_stmt *stmt;
	char buf[BUFFER_SIZE];
	
	db_err = sqlite3_open("server.db", &db);
	
	// On indique au serveur que l'on va effectuer l'upload de la liste des fichiers.
	strcpy(buf, "uploadfilelist");
	sock_err = send(sock, buf, BUFFER_SIZE, 0);
	if(sock_err == SOCKET_ERROR) return -1;
	
	/* On envoie les fichiers les uns après les autres, puis un signal de fin lorsque l'on a terminé.
	 * Le serveur central peut ainsi stopper la réception. */
	query = sqlite3_mprintf("SELECT * FROM Fichiers;");
	
	sqlite3_prepare(db, query, -1, &stmt, NULL);
	
	db_err = sqlite3_step(stmt);
	
	// Tant qu'il reste des tuples à lire.
	while(db_err == SQLITE_ROW)
	{
		strcpy(buf, (char*)sqlite3_column_text(stmt, 0));	// Lecture et envoi du nom du fichier.
		sock_err = send(sock, buf, BUFFER_SIZE, 0);
		if(sock_err == SOCKET_ERROR) return -1;
		
		strcpy(buf, (char*)sqlite3_column_text(stmt, 1));	// Lecture et envoi de la description.
		sock_err = send(sock, buf, BUFFER_SIZE, 0);
		if(sock_err == SOCKET_ERROR) return -1;
		
		db_err = sqlite3_step(stmt);
	}
	
	strcpy(buf, "endoffilelist");	// Envoi du signal de fin.
	sock_err = send(sock, buf, BUFFER_SIZE, 0);
	if(sock_err == SOCKET_ERROR) return -1;
	
	sqlite3_close(db);
	
	return 0;
}
