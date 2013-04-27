#include "util.h"

int handle_client(SOCKET sock, SOCKADDR_IN sin, sqlite3 *db);
int check_client(SOCKET sock, SOCKADDR_IN sin, sqlite3 *db);
int add_client(SOCKET sock, SOCKADDR_IN sin, sqlite3 *db);
int receive_shared_files(SOCKET sock, SOCKADDR_IN sin, sqlite3 *db);
int disconnect_client(SOCKADDR_IN sin, sqlite3 *db);
int get_owner(SOCKET sock, SOCKADDR_IN sin, sqlite3 *db);
