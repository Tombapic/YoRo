#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sqlite3.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define PORT 2013
#define BUFFER_SIZE 512

typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;

int create_account(SOCKET sock);
int authenticate(SOCKET sock);
int check_client(SOCKET sock);
int update_shared_files();
int send_shared_files(SOCKET sock);

int main(int argc, char *argv[])
{
	if(argc != 2)
	{
		printf("Utilisation : server <ip_du_serveur>\n");
		return EXIT_FAILURE;
	}
	
	system("clear");
	printf("--------------------------------------------------------------------------------");
	printf("                                 SERVEUR LOCAL\n");
	printf("--------------------------------------------------------------------------------");
	
	int sock_err;
	int user_choice;
	char *server_addr = argv[1];
	
	// Socket et contexte d'adressage du serveur.
	SOCKET sock;
	SOCKADDR_IN sin;
	socklen_t ssize = sizeof(sin);
	
	// Socket et contexte d'adressage du serveur central.
	SOCKET main_sock;
	SOCKADDR_IN main_sin;
	socklen_t main_size = sizeof(main_sin);
	
	// Socket et contexte d'adressage du client.
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
	sin.sin_port = htons(PORT);
	
	main_sin.sin_addr.s_addr = inet_addr(server_addr);
	main_sin.sin_family = AF_INET;
	main_sin.sin_port = htons(PORT);
	
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
	
	/* UNE FOIS QUE LE SERVEUR EST INITIALISÉ. */
	sock_err = send(sock, "initok", BUFFER_SIZE, 0);
	
	// A compléter !
	// 5 est le nombre maximal de connexions pouvant être mises en attente.
	sock_err = listen(sock, 5);
	
	if(sock_err == SOCKET_ERROR)
	{
		printf("Erreur : echec de l'ecoute de la socket.\n");
		return EXIT_FAILURE;
	}
	
	close(sock);
	
	return EXIT_SUCCESS;
}

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
		
		while(dp != NULL)
		{
			if(strcmp(dp->d_name, (char*)sqlite3_column_text(stmt, 0)))
				found = 1;
			
			dp = readdir(dir);
		}
		
		// Si le fichier n'a pas été trouvé, on le supprime de la base.
		if(found == 0)
		{
			query = sqlite3_mprintf("DELETE FROM Fichiers WHERE chemin = '%q';", (char*)sqlite3_column_text(stmt, 0));
			db_err = sqlite3_exec(db, query, NULL, 0, &zErrMsg);
		}
		
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
			 * si le fichier existe déjà, il ne sera pas ajouté.
			 * EN FAIT SI ! QUAND ON ENVOIE UNE REQUÊTE VIA UN PROGRAMME, CELA VIOLE VISIBLEMENT
			 * EN TOUTE IMPUNITÉ LES CONTRAINTES DE CLÉS PRIMAIRES ! */
			
			//////////////////////////// CORRIGER ICI !
			
			// On vérifie que le fichier n'est pas déjà présent dans la base.
			// Récupération de l'identifiant du propriétaire du fichier.
			query = sqlite3_mprintf("SELECT * FROM Fichiers WHERE chemin = '%q';", dp->d_name);
			sqlite3_prepare(db, query, -1, &stmt, NULL);
			db_err = sqlite3_step(stmt);
			
			//~ if(db_err == SQLITE_EMTPY) printf("pouet !\n");
			
			// Préparation de la requête. Pour le moment, on n'entre aucune description.
			//query = sqlite3_mprintf("INSERT ON CONFLICT IGNORE INTO Fichiers VALUES('%q', '');", dp->d_name);
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
