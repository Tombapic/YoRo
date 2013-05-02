#include "util.h"

int handle_user(SOCKET sock, SOCKADDR_IN sin);
int handle_peer(SOCKET sock, SOCKADDR_IN sin);
int download_file(SOCKET sock);
void print_header();
int search_request(SOCKET sock);
int cut(char** tableau,char* source);
int receive_search_result(SOCKET sock,int nbr_files);
void add_description(SOCKET sock);
