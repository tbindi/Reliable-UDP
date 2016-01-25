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
#include "ServerSock.h"

using namespace std;

ServerSockUDP::ServerSockUDP(int domain,int type,int protocol,int port){
	this->port_number = port;
	this->server_sock_fd = -1;
	this->socket_server(domain,type,protocol);
	if(this->bind_server() > 0){
		cout << "Binding successfully done...." << endl;
	}
}

int ServerSockUDP::get_socket(){
	return this->server_sock_fd;
}

int ServerSockUDP::socket_server(int domain, int type, int protocol) {

	this->server_sock_fd = socket(domain, type, protocol);

	if (this->server_sock_fd < 0) {
		cerr << "Error in opening socket: " << strerror(errno) << endl;
		exit(0);
	}

	return 1;
}

int ServerSockUDP::bind_server(){

	struct sockaddr_in server_address;
	
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = inet_addr("127.0.0.1"); // localhost
	server_address.sin_port = htons(this->port_number);
	
	if( bind(this->server_sock_fd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0 ){
		cerr << "Error in binding: " << endl;
		cout << "Error Code: " << strerror(errno) << endl;
		exit(1);
	}

	return 1;
}
