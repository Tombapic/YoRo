#include "util.h"

int handle_user(SOCKET sock, SOCKADDR_IN sin);
int handle_peer(SOCKET sock, SOCKADDR_IN sin);
int download_file(SOCKET sock);
void print_header();
