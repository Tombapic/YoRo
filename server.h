#include "util.h"

int create_account(SOCKET sock);
int authenticate(SOCKET sock);
int check_client(SOCKET sock);
int update_shared_files();
int send_shared_files(SOCKET sock);
