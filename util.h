#include <sqlite3.h>

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define SERVER_PORT 2013
#define HOST_PORT 2014

// La taille du buffer s'aligne sur celle des paquets (512 octets).
#define BUFFER_SIZE 512

typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
