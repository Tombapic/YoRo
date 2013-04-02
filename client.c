#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define PORT 2013
#define BUFFER_SIZE 512
#define SERVER_ADDR "127.0.0.1"

typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;

int main(int argc, char *argv[])
{
	int sock_err;
	
	SOCKET sock;
	SOCKADDR_IN sin;
	socklen_t ssize = sizeof(sin);
	
	sock = socket(AF_INET, SOCK_STREAM, 0);
	
	if(sock == INVALID_SOCKET)
	{
		printf("Erreur : echec de la creation de la socket.\n");
		return EXIT_FAILURE;
	}
	
	// Paramétrage de la socket.
	sin.sin_addr.s_addr = inet_addr(SERVER_ADDR);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(PORT);
	
	sock_err = connect(sock, (SOCKADDR*)&sin, ssize);
	
	if(sock_err == SOCKET_ERROR)
	{
		printf("Erreur : echec de la connexion au serveur.\n");
		return EXIT_FAILURE;
	}
	
	printf("Connexion à %s sur le port %d\n", inet_ntoa(sin.sin_addr), htons(sin.sin_port));
	
	close(sock);
	
	return EXIT_SUCCESS;
}
