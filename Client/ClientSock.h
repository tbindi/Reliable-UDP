#include <stdio.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <cstring>
#include <string>
#include <ctime>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include <errno.h>

using namespace std;

class ClientSockUDP
{
	int client_sock_fd;
public:
	int port_number;
	string server_host;

	~ClientSockUDP();
	ClientSockUDP(int domain,int type,int protocol,int port,string server);
	int socket_client(int domain,int type,int protocol);
	struct sockaddr_in setup_server();
	int get_socket();
};
