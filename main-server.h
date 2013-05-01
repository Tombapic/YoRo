#include "util.h"

int handle_client(SOCKET sock, SOCKADDR_IN sin);
int check_client(SOCKET sock, SOCKADDR_IN sin);
int add_client(SOCKET sock, SOCKADDR_IN sin);
int receive_shared_files(SOCKET sock, SOCKADDR_IN sin);
int disconnect_client(SOCKADDR_IN sin);
int get_owner(SOCKET sock, SOCKADDR_IN sin);
int search(SOCKET sock,char** search_res);
int send_search_result(SOCKET sock, int nbr_files,char ** search_res);
