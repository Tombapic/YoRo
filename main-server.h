#include "util.h"

int handle_client(SOCKET sock, SOCKADDR_IN sin);
int check_client(SOCKET sock, SOCKADDR_IN sin);
int add_client(SOCKET sock, SOCKADDR_IN sin);
int receive_shared_files(SOCKET sock, SOCKADDR_IN sin);
int disconnect_client(SOCKADDR_IN sin);
int get_owner(SOCKET sock, SOCKADDR_IN sin);
