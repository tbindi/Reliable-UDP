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

#include "ClientSock.h"

using namespace std;

int ClientSockUDP::socket_client(int domain,int type,int protocol){

	this->client_sock_fd = socket(domain,type,protocol);

	if(this->client_sock_fd < 0){
		cerr << "Error in opening socket: " << strerror(errno) << endl;
		exit(2);
	}

	return 1;
}

ClientSockUDP::ClientSockUDP(int domain,int type,int protocol,int port,string server){
	this->port_number = port;
	this->client_sock_fd = -1;
	this->server_host = server;

	this->socket_client(domain,type,protocol);
}

struct sockaddr_in ClientSockUDP::setup_server(){

	struct sockaddr_in server_address;

	if(inet_addr(this->server_host.c_str()) == -1){
		// hostname to ip address
		struct hostent *host;

		if( (host = gethostbyname(this->server_host.c_str()) ) == NULL )
		{
			cerr << "Error resolving hostname " << this->server_host << endl;
			cout << "Error Code: " << strerror(errno) << endl;
			exit(3);
		}

		struct in_addr **address_list;

		address_list = (struct in_addr **) host->h_addr_list;
		
		for( int i = 0; address_list[i] != NULL; i++ ){
			server_address.sin_addr = *address_list[i];
			break;
		}

	}else{
		// plain ip address
		server_address.sin_addr.s_addr = inet_addr(this->server_host.c_str());
	}
	
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(this->port_number);

	return server_address;
}

int ClientSockUDP::get_socket(){
	return this->client_sock_fd;
}

ClientSockUDP::~ClientSockUDP() {
	close (this->client_sock_fd);
}
