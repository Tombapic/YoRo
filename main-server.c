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

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define PORT 2013

// La taille du buffer s'aligne sur celle des paquets (512 octets).
#define BUFFER_SIZE 512
#define SERVER_ADDR "127.0.0.1"

typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;

int handle_client(SOCKET sock, SOCKADDR_IN sin, sqlite3 *db);
int check_client(SOCKET sock, SOCKADDR_IN sin, sqlite3 *db);
int add_client(SOCKET sock, SOCKADDR_IN sin, sqlite3 *db);
int receive_shared_files(SOCKET sock, SOCKADDR_IN sin, sqlite3 *db);

int main(int argc, char *argv[])
{
	system("clear");
	printf("--------------------------------------------------------------------------------");
	printf("                                SERVEUR CENTRAL\n");
	printf("--------------------------------------------------------------------------------");
	
	// Pointeur sur la base de données du serveur.
	sqlite3 *db;
	int db_err;
	
	db_err = sqlite3_open("main-server.db", &db);
	
	if(db_err)
	{
		printf("Erreur : echec de l'ouverture de la base de donnees.\n");
		return EXIT_FAILURE;
	}
	
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
	sin.sin_port = htons(PORT);
	
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
				handle_client(csock, csin, db);
				close(csock);
				kill(getpid(), SIGTERM);
			
			default :	// Père.
				/* Se renseigner sur le WNOHANG.
				 * Lorsqu'un fils est tué, il passe en zombie jusqu'à ce que le père se termine.
				 * Régler ça et tuer le fils directement ! */
				waitpid(-1, &status, WNOHANG);
		}
	}
	
	close(sock);
	sqlite3_close(db);
	
	return EXIT_SUCCESS;
}

/**
 * Gère un nouveau client.
 * 
 * Paramètres :
 * - sock	: la socket sur laquelle communiquer avec le client.
 * - sin	: le contexte d'adressage de cette socket.
 * - db		: un pointeur sur la base de données du serveur.
 * 
 * Retour : 0 si succès, -1 sinon.
 **/
int handle_client(SOCKET sock, SOCKADDR_IN sin, sqlite3 *db)
{
	char buf[BUFFER_SIZE];
	int sock_err;
	
	sock_err = recv(sock, buf, BUFFER_SIZE, 0);
	if(sock_err == SOCKET_ERROR) return -1;
	
	// Tant que le client est connecté, on le gère.
	while(strcmp(buf, "disconnect") != 0)
	{
		printf("ça boucle ?\n");
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
				sock_err = check_client(sock, sin, db);
				sock_err = recv(sock, buf, BUFFER_SIZE, 0);
			}
		}
		
		// Si le client demande la création d'un compte.
		else if(strcmp(buf, "add") == 0)
		{
			sock_err = -1;
			while(sock_err != 0) sock_err = add_client(sock, sin, db);
		}
		
		// Si le client uploade la liste de ses fichiers partagés.
		else if(strcmp(buf, "uploadfilelist") == 0)
		{
			sock_err = receive_shared_files(sock, sin, db);
			if(sock_err == -1) return -1;
		}
		
		///////////////// TEST ///////////////////
		else if(strcmp(buf, "initok") == 0) { return 10; }
		
		/* RAJOUTER TOUTES LES FONCTIONS DE GESTION ICI. */
		
		sock_err = recv(sock, buf, BUFFER_SIZE, 0);
		if(sock_err == SOCKET_ERROR) return -1;
		
		/* Gestion de l'arrêt brutal d'un client.
		 * Si le client se déconnecte sans fermer la socket, recv retourne 0.
		 * Si on ne gère pas ce cas, le serveur tourne en boucle avec le dernier envoi du client
		 * et ne répond plus. */
		if(sock_err == 0)
		{
			printf("Arret critique du client.\n");
			return -1;
		}
	}
	
	/* ECRIRE ICI LA FONCTION DE DÉCONNEXION QUI PURGE LA BASE DES FICHIERS DE L'UTILISATEUR. */
	
	return 0;
}


/**
 * Vérifie l'identité d'un client du réseau à partir de la base de données.
 * 
 * Paramètre :
 * - sock	: la socket sur laquelle reçevoir les informations d'authentification.
 * - sin	: le contexte d'adressage de cette socket.
 * - db		: un pointeur sur la base de données du serveur.
 * 
 * Retour : 0 si succès, -1 sinon.
 **/
int check_client(SOCKET sock, SOCKADDR_IN sin, sqlite3 *db)
{
	int sock_err;
	char *query;
	sqlite3_stmt *stmt;
	char password_db[20];	// Le mot de passe contenu dans la base.
	
	char buf[BUFFER_SIZE];
	char login[BUFFER_SIZE];
	char password[BUFFER_SIZE];
	
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
							inet_ntoa(sin.sin_addr), login, inet_ntoa(sin.sin_addr));
		
		sqlite3_exec(db, query, NULL, 0, NULL);
		
		return 0;
	}
	
	else
	{
		strcpy(buf, "authko");
		sock_err = send(sock, buf, BUFFER_SIZE, 0);
		printf("Echec de l'authentification.\n");
		return -1;
	}
}

/**
 * Ajoute un client à la base de données.
 * 
 * Paramètres :
 * - sock	: la socket sur laquelle reçevoir les informations d'authentification.
 * - sin	: le contexte d'adressage de cette socket.
 * - db		: un pointeur sur la base de données du serveur.
 * 
 * Retour : 0 si succès, -1 sinon.
 **/
int add_client(SOCKET sock, SOCKADDR_IN sin, sqlite3 *db)
{
	int db_err;
	int sock_err;
	char *zErrMsg = 0;
	char *query;
	char buf[BUFFER_SIZE];
	char login[BUFFER_SIZE];
	char password[BUFFER_SIZE];
	
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
		printf("Echec de la creation du compte. L'identifiant est deja present dans la base.\n");
		
		strcpy(buf, "addko");
		sock_err = send(sock, buf, BUFFER_SIZE, 0);
		
		if(sock_err == SOCKET_ERROR) return -1;
		
		return -1;
	}
	
	else
	{
		// On confirme au client que l'ajout à la base s'est bien déroulé.
		strcpy(buf, "addok");
		sock_err = send(sock, buf, BUFFER_SIZE, 0);
		
		if(sock_err == SOCKET_ERROR) return -1;
	}
	
	return 0;
}

/**
 * Insère les fichiers du client nouvellement connecté à la base de données.
 * 
 * Paramètres :
 * - sock	: la socket sur laquelle est connecté le client
 * - sin	: le contexte d'adressage de cette socket.
 * - db		: un pointeur sur la base de données du serveur.
 * 
 * Retour : 0 si succès, -1 sinon.
 **/
int receive_shared_files(SOCKET sock, SOCKADDR_IN sin, sqlite3 *db)
{
	int sock_err, db_err;
	char *query, *zErrMsg;
	sqlite3_stmt *stmt;
	char path[BUFFER_SIZE];
	char description[BUFFER_SIZE];
	
	printf("Un client uploade ses fichiers.\n");
	
	// Récupération de l'identifiant du propriétaire du fichier.
	query = sqlite3_mprintf("SELECT id FROM Utilisateurs WHERE ip = '%q';", inet_ntoa(sin.sin_addr));
	sqlite3_prepare(db, query, -1, &stmt, NULL);
	sock_err = sqlite3_step(stmt);	// Exécution de la requête.
	
	// Si aucun identifiant n'a été récupéré (ne devrait jamais arriver).
	if((char*)sqlite3_column_text(stmt, 0) == NULL)
	{
		printf("Erreur : utilisateur inconnu.\n");
		return -1;
	}
	
	sock_err = recv(sock, path, BUFFER_SIZE, 0);
	if(sock_err == SOCKET_ERROR) return -1;
	
	printf("%s\n", path);
	
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
		 * Si un veut télécharger fic.txt et que deux utilisateurs le proposent, on prend le premier qui vient...
		 * trouver le proprio grâce à l'ip dans la base
		 * PAS BESOIN DE LA TAILLE ! ON LA PASSERA JUSTE LORSQUE QUELQU'UN EN AURA BESOIN EN LA CALCULANT DIRECTEMENT
		 * SUR LE FICHIER ! */	
		
		query = sqlite3_mprintf("INSERT INTO Fichiers VALUES('%q', '%q', '%q');",
								path, (char*)sqlite3_column_text(stmt, 0), description);
		db_err = sqlite3_exec(db, query, NULL, 0, &zErrMsg);
		
		if(db_err != SQLITE_OK)
		{
			printf("Erreur SQL : %s\n", zErrMsg);
			return -1;
		}
		
		// Réception du tuple suivant.
		sock_err = recv(sock, path, BUFFER_SIZE, 0);
		if(sock_err == SOCKET_ERROR) return -1;
	}
	
	return 0;
}
