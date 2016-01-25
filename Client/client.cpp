#include <stdio.h>
#include <iostream>
#include <bitset>
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
#include <deque>
#include "ClientSock.h"
#include "RudpSegment.h"

using namespace std;

#define MTU 1472
#define HDR_LEN 12
#define TRUE 'Y'
#define FALSE 'N'
#define ACK "This is ACK"
#define NONE 0
#define DROP 1
#define DELAY 2
#define BOTH 3

typedef unsigned char byte;
typedef unsigned int byte4;
typedef unsigned short byte2;

class RecvBuffer{
public:
	int sequence_number;
	byte* data;
	RecvBuffer(int sequence_number,byte* data);
	byte4 get_ack();
};

RecvBuffer::RecvBuffer(int sequence_number,byte* data){
	this->sequence_number = sequence_number;
	this->data = data;
}

byte4 RecvBuffer::get_ack(){
	return this->sequence_number+strlen((char *)data);
}

byte* init_request(byte* filename){
	byte4 sequence_number = rand() % 100;
	byte4 ack_number = 10;
	byte ack_flag = FALSE;
	byte fin_flag = FALSE;
	RudpSegment* request = new RudpSegment(ack_number,ack_flag,filename,sequence_number,fin_flag);

	return request->prepare_segment();
}

byte * prepare_ack(byte4 ack_number,byte ack_flag){
	RudpSegment* send_segment = new RudpSegment(ack_number,ack_flag,(byte *)ACK);
	return send_segment->prepare_segment();
}

RudpSegment* parse_request(byte* buffer){

	byte4 seq_no = (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | (buffer[0]);
	byte4 ack_no = (buffer[7] << 24) | (buffer[6] << 16) | (buffer[5] << 8) | (buffer[4]);
	byte ack_flag = (byte)buffer[8];
	byte fin_flag = (byte)buffer[9];
	byte2 length = (buffer[11] << 8) | (buffer[10]);
	byte* data = &buffer[12];

	return new RudpSegment(ack_no,ack_flag,data,seq_no,fin_flag);
}

RecvBuffer* get_recvbuffer(byte* buffer){
	RudpSegment* temp = parse_request(buffer);
	return new RecvBuffer(temp->sequence_number,temp->data);
}

int send_packet(byte* buffer,int client_sock_fd,struct sockaddr_in server_details){

	int n = sendto(client_sock_fd,buffer,MTU,0,(struct sockaddr *)&server_details,sizeof(server_details));

	if( n < 0){
		cerr << "Error in sending to socket: " << strerror(errno) << endl;
		exit(1);
	}

	return n;
}


// Recv should store out of order packets in the queue and send acks
bool recv_response(int sock_fd,struct sockaddr_in server_details,size_t window_size,int mode,int percentage){
	// get response in a while loop break when you get FIN.
	byte* recv_buffer = (byte *) calloc(MTU,sizeof(byte));
	memset(recv_buffer,0,MTU);
	byte* send_buffer = (byte *) calloc(MTU,sizeof(byte));
	memset(send_buffer,0,MTU);
	ostringstream r_stream;
	RudpSegment* recv_segment;
	bool flag = true;
	byte4 fin_flag = FALSE;
	int last_byte_read = -1;
	int last_byte_rcvd = -1;

	deque<RecvBuffer*> recv_queue;
	recv_queue.clear();

	RecvBuffer* empty = new RecvBuffer(-1,(byte*)" ");

	while(1){
		if(recvfrom(sock_fd,recv_buffer,MTU,0,NULL,NULL)){
			RudpSegment* temp = parse_request(recv_buffer);

			// Implement Drop / Sleep
			if(mode != NONE){
				if(rand() % 100+1 <= percentage){ // Percentage of drop or delay
					if(mode == DROP) continue;
					else if(mode == DELAY) usleep( (rand() % 10)*1000000 );
					else if(mode == BOTH) {
						if(rand() % 2 == 0) continue;
						else usleep( (rand() % 10)*1000000 );
					}
				}	
			}

			if(temp->fin_flag == TRUE)
				fin_flag = TRUE;

			// Always send ACK for last byte read + 1
			if(last_byte_read == -1 && last_byte_rcvd == -1){
				// First packet rcvd. Initial Read.
				recv_queue.push_back(new RecvBuffer(temp->sequence_number,temp->data));
				last_byte_rcvd = temp->sequence_number+(MTU-HDR_LEN)-1;

			}else if( last_byte_read+1 == temp->sequence_number){
				// Next expected packet arrival.
				if(recv_queue.size() == 0){
					recv_queue.push_back(new RecvBuffer(temp->sequence_number,temp->data));
				}else{
					recv_queue.pop_front();
					recv_queue.push_front(new RecvBuffer(temp->sequence_number,temp->data));
				}
				last_byte_rcvd = temp->sequence_number+(MTU-HDR_LEN)-1;

			}else if( last_byte_read+1 != temp->sequence_number){
				// Out of order packet
				int diff =  (int)( ((temp->sequence_number) - (last_byte_read+1))/(MTU-HDR_LEN) );

				if( diff > window_size-1){
					// if out of order packet is greater than rcv window discard packet.
					continue;
				}

				if(diff >= recv_queue.size()){
					// Out of order cases:
					// 1. [-1,X] ==> [-1,X,Y] - tested
					// 2. [-1,X] ==> [-1,X,-1,Y] - tested
					// 3. [] ==> [-1,-1,X] - tested
					recv_queue.resize(diff+1,empty);
					recv_queue.at(diff) = new RecvBuffer(temp->sequence_number,temp->data);
				}else{
					// Out of order cases:
					// 1. [-1,X,-1,Z] ==> [-1,X,Y,Z] - tested
					recv_queue.at(diff) = new RecvBuffer(temp->sequence_number,temp->data);
				}

				last_byte_rcvd = temp->sequence_number+(MTU-HDR_LEN)-1;

			}else if( last_byte_read+1 > temp->sequence_number){
				continue;
			}

			// check for order, update last read
			if(recv_queue.front()->sequence_number != -1){
				while(!recv_queue.empty()) {

					if(recv_queue.front()->sequence_number == -1)
						break;

					r_stream << recv_queue.front()->data;
					last_byte_read = recv_queue.front()->sequence_number+(MTU-HDR_LEN)-1;
					recv_queue.pop_front();

				}
			}


			send_buffer = prepare_ack(last_byte_read+1,TRUE);
			send_packet(send_buffer,sock_fd,server_details);
			memset(recv_buffer,0,MTU);
			// Last packet recieved & nothing to process in the window.
			if(fin_flag == TRUE && recv_queue.size() == 0){
				flag = false;
			}

			delete temp;
		}

		if(!flag){
			break;
		}

	}
	ofstream out_file;
	out_file.open("out_file.txt");
	if(out_file.is_open())
		out_file << r_stream.str();
	else
		cout << " Unable to Open File" << endl;
	out_file.close();

	return true;
}

int main(int argc, char const *argv[]){
	string server_host;
	byte* filename;
	int port,mode=-1,percentage=0;
	struct sockaddr_in server_details;
	size_t window_size = 0;

	if(argc != 7){
		// Client hostname portnumber filename
		cout << "Not Enough Arguments, Please enter arguments in format:" << endl;
		cout << "client <server_host/ip> <server_port> <filename.txt> <window_size> <none/drop/delay> <percentage>" << endl;
		cout << "<window_size> 		: input in terms of MSS" << endl;
		cout << "<drop/delay> 		: 0 -> None, 1 -> Drop, 2 -> Delay" << endl;
		cout << "<percentage>		: Integer,(0-100) Ex: 33, 33 out of 100 packets will be dropped" << endl;
		exit(0);
	}else if(argc == 7){
		// Assign arguments
		server_host = argv[1];
		port = atoi(argv[2]);
		filename = (byte *)argv[3];
		window_size = atoi(argv[4]);
		if(atoi(argv[5]) >= 0 && atoi(argv[5]) < 4){
			mode = atoi(argv[5]);
		}else{
			cout << "Not Enough Arguments, Please enter arguments in format:" << endl;
			cout << "client <server_host/ip> <server_port> <filename.txt> <window_size> <none/drop/delay> <percentage>" << endl;
			cout << "<window_size> 		: input in terms of MSS" << endl;
			cout << "<drop/delay> 		: 0 -> None, 1 -> Drop, 2 -> Delay" << endl;
			cout << "<percentage>		: Integer,(0-100) Ex: 33, 33 out of 100 packets will be dropped" << endl;
			exit(0);
		}
		percentage = atoi(argv[6]);
	}


	ClientSockUDP* client = new ClientSockUDP(AF_INET, SOCK_DGRAM, 0, port, server_host);
	server_details = client->setup_server();

	// init request -> one module
	byte* buffer = init_request(filename);

	// send request -> one module initial request
	send_packet(buffer,client->get_socket(),server_details);

	// recv all the response
	recv_response(client->get_socket(),server_details,window_size,mode,percentage);

	// Free buffer and Close
	free(buffer);
}