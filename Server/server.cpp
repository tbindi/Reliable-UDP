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
#include <cmath>
#include <time.h>
#include "ServerSock.h"
#include "Segment.h"

using namespace std;

#define MTU 1472
#define HDR_LEN 12
#define TRUE 'Y'
#define FALSE 'N'
#define APP 'A'
#define TIMER 'B'
#define ACK 'C'
#define NANO_SEC 1000000000
#define MICR_SEC 1000000
#define INIT_SEC 50000

typedef unsigned char byte;
typedef unsigned int byte4;
typedef unsigned short byte2;

class Sliding{
public:
	RudpSegment* rudp;
	double sent_time;
	byte4 seq_no;
	bool mark;
	Sliding(RudpSegment* rudp,double sent_time);
	~Sliding();
};

Sliding::Sliding(RudpSegment* rudp,double sent_time){
	this->rudp = rudp;
	this->mark = false;
	this->sent_time = sent_time;
	this->seq_no = rudp->sequence_number;
}

class Congestion{
public:
	double cwnd;
	double ssthresh;
	size_t advertised_size;
	int dupAck;
	bool is_slow; // yes -> slow start, no -> congestion avoidance
	double estimated_rtt;
	double dev_rtt;
	double timeout_interval;
	Congestion(size_t advertised_size);
	~Congestion();
	void timeout();
	void update_rtt(double start,double end);
	void slow_start();
	void ss_update();
	void ca_update();
};

Congestion::Congestion(size_t advertised_size){
	// slow start init
	this->advertised_size = advertised_size;
	this->cwnd = 1;
	this->ssthresh = 64;
	this->dupAck = 0;
	this->is_slow = true;
	this->estimated_rtt = INIT_SEC;
	this->dev_rtt = 0;
	this->timeout_interval = 0;
}

void Congestion::update_rtt(double start,double end){ // start and end time to calculate sample & continues.
	double sample_rtt = end - start;
	estimated_rtt = (0.875 * estimated_rtt) + (0.125 * sample_rtt);
	dev_rtt = (0.75 * dev_rtt) + (0.25 * abs(sample_rtt-estimated_rtt));
	timeout_interval = estimated_rtt + 4 * dev_rtt;
}

void Congestion::slow_start(){
	this->ssthresh = this->cwnd/2;
	this->cwnd = 1; // resize congestion window to 1
	this->is_slow = true; // slow start
	this->dupAck = 0;
}

void Congestion::ss_update(){
	this->cwnd *= 2;
	this->dupAck = 0;
	this->is_slow = true;
}

void Congestion::ca_update(){
	this->cwnd += 1;
	this->dupAck = 0;
	this->is_slow = false;
}

RudpSegment* parse_request(byte* buffer){
	byte4 seq_no = (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | (buffer[0]);
	byte4 ack_no = (buffer[7] << 24) | (buffer[6] << 16) | (buffer[5] << 8) | (buffer[4]);
	byte ack_flag = (unsigned char)buffer[8];
	byte fin_flag = (char)buffer[9];
	byte2 length = (buffer[11] << 8) | (buffer[10]);
	byte* data = &buffer[12];
	return new RudpSegment(seq_no,fin_flag,data,ack_no,ack_flag);
}

bool send_response(int sock_fd,byte* buffer,struct sockaddr_storage client_details){
	socklen_t client_length = sizeof(client_details);
	if(sendto(sock_fd,buffer,MTU,0,(struct sockaddr *)&client_details, client_length)){
		return true;
	}
	return false;
}

byte4 read_ack(int sock_fd,struct sockaddr_storage client_details){
	byte* buffer = (byte *) calloc(MTU,sizeof(byte));
	socklen_t client_length = sizeof(client_details);
	memset(buffer,0,MTU);

	if(recvfrom(sock_fd,buffer,MTU,0,(struct sockaddr *)&client_details,&client_length)){
		RudpSegment* temp = parse_request(buffer);
		if(temp->ack_flag == TRUE){
			free(buffer);
			return temp->ack_number;
		}
		delete temp;
	}
	free(buffer);
	return 0;
}

double get_time(){
	struct timespec start;
	clock_gettime(CLOCK_MONOTONIC,&start);
	return start.tv_sec + start.tv_nsec / NANO_SEC;
}

void retransmit(byte4 seq_no,int sock_fd,struct sockaddr_storage client_details,Segment* seg){
	byte* buffer = (byte *) calloc(MTU,sizeof(byte));
	memset(buffer,0,MTU);
	buffer = seg->get_segment(seq_no);
	send_response(sock_fd,buffer,client_details);
	memset(buffer,0,MTU);
	free(buffer);
}

bool send_segments(byte* filename,int sock_fd,struct sockaddr_storage client_details,size_t recv_window){
	byte4 init_seq_no = rand() % 100;
	byte4 seq_no = init_seq_no;
	byte4 send_base = init_seq_no;
	bool flag = true;
	int sent = 0, left = 0;

	byte* buffer = (byte *) calloc(MTU,sizeof(byte));
	memset(buffer,0,MTU);
	
	// always returns the packet from a sequence number
	Segment* seg = new Segment(init_seq_no);
	seg->open_file(filename);

	Congestion* congestion = new Congestion(recv_window);
	deque<Sliding*> window;
	window.clear();
	int packet_dropped = 0;
	int packet_number = ((int) seg->file_length / (MTU-HDR_LEN)) + 1;

	fd_set read_fds;
	struct timeval tv;
	FD_ZERO(&read_fds);

	FD_SET(sock_fd,&read_fds);
	int n = sock_fd + 1;
	byte4 ack_no;

	tv.tv_sec = 0;
	tv.tv_usec = INIT_SEC;
	int rv,eff_wind;

	while(1) {
	    
	    eff_wind = min(congestion->cwnd,(double) recv_window);

	    if(congestion->is_slow){
	    	cout << "-----------	Sending Packets in mode: Slow Start 	-----------" << endl;
	    	cout << "\tMaximum Packets that will be sent:" << eff_wind << endl;
	    }else{
	    	cout << "-----------	Sending Packets in mode: Congestion Avoidance 	-----------" << endl;
	    	cout << "\tMaximum Packets that will be sent:" << eff_wind << endl;
	    }

	    cout << "\tTimeout Interval: " << congestion->timeout_interval << endl;
	    window.clear();

	    while(eff_wind != 0 && (seq_no-init_seq_no) < seg->file_length) {
	    	buffer = seg->get_segment(seq_no);
	    	send_response(sock_fd,buffer,client_details);
	        window.push_back(new Sliding(seg->get_Rudp(seq_no),get_time()));
	        seq_no += (MTU-HDR_LEN);
	        eff_wind -= 1;
	    }

	    sent = window.size();

		FD_ZERO(&read_fds);
		FD_SET(sock_fd,&read_fds);
		if(tv.tv_usec != 0 && tv.tv_sec != 0){
		    tv.tv_sec = (long) congestion->timeout_interval / NANO_SEC;
		    tv.tv_usec = (congestion->timeout_interval) * 1000 - tv.tv_sec;
	    }

	    while(1){

	    	rv = select(n,&read_fds,NULL,NULL,&tv);

	    	if(rv == -1){
	    		// Case: Error
		    	cout << "Error in Select" << endl;
		    	cerr << strerror(errno) << endl;
		    	exit(5);
	    	}else if(rv > 0){
		    	// Case: ACK		    	
		    	ack_no = read_ack(sock_fd,client_details);
		    	cout << "### Ack Event " << endl;
	    		if(send_base < ack_no){
	    			// new ACK
	    			cout << "\tNew ACK: " << ack_no << endl;
					send_base = ack_no;
					if(window.size() > 0 && send_base-(MTU-HDR_LEN) >= window.front()->seq_no){
						while( !window.empty() ) {
							if(window.front()->seq_no < send_base && window.front()->mark == false){
								congestion->update_rtt(window.front()->sent_time, get_time());
								window.pop_front();
							}else{
								break;
							}
						}
					}
					
		    		if(window.size() == 0){
		    			left = 0;
		    			if(congestion->cwnd >= congestion->ssthresh && congestion->ssthresh != -1){
			    			congestion->is_slow = false; // congestion avoidance
			    			cout << "**** Entering Congestion Avoidance mode ****" << endl;
		    			}
		    			if(congestion->is_slow) congestion->ss_update();
		    			else congestion->ca_update();
		    			break;
		    		}

				}else{
					// dup ACK
					congestion->dupAck++;
					cout << "\tDup ACK: " << ack_no << " ACK Count:" << congestion->dupAck << endl;
 					if( congestion->dupAck >=  3){
						// retransmit & mark to not calculate timeout
						retransmit(ack_no,sock_fd,client_details,seg);
						packet_dropped += 1;
						cout << "\tRetransmitting : " << ack_no << endl;
						if(window.size() != 0 ){
							if( !window.front()->mark){
								for (int i = 0; i < window.size()-1; i++){
									window[i]->mark = true;
								}
							}
						}
						congestion->slow_start(); // Back to Slow Start
						congestion->dupAck = 0;
						left = window.size();
					}
				}
		    }else if( rv == 0 ){
		    	// Timeout
	    		cout << "### Timeout Event " << endl;
		    	if(tv.tv_sec == 0 && tv.tv_usec == 0){
		    		// Loss occurred go back to slow start
		    		packet_dropped += window.size();
		    		if(window.size() != 0){
		    			left = window.size();
			    		retransmit(ack_no,sock_fd,client_details,seg);
			    		cout << "\tRetransmitting : " << window.front()->seq_no << endl;
			    		window.clear();
					}

					congestion->slow_start();
		    	}
		    	break;
		    }
	    }

	    cout << "\t\tPercentage of Packets Sent: " << sent-left << "/" << sent << " = " << ((float)(sent-left)/sent) * 100 << endl;
	    sent = 0, left = 0;
	    if((seq_no-init_seq_no) > seg->file_length || (ack_no-init_seq_no) > seg->file_length ){ 
	    	// check for window size before break.
	    	// Last Packet has been sent.
	    	cout << endl << "Last Packet has been delivered & All ACKs have been received" << endl;
	    	// call another function to make sure all ack has been recvd.
	    	break;
	    }
	}

	memset(buffer,0,MTU);
	delete seg;
	return flag;
}

int main(int argc, char const *argv[]){
	int port;
	size_t recv_window;
	socklen_t client_length;
	struct sockaddr_storage client_details;
	byte buffer[MTU];
	bzero(buffer,MTU);

	if( argc != 3){
		// Client hostname portnumber filename
		cout << "Not Enough Arguments, Please enter arguments in format:" << endl;
		cout << "server <server_port> <recv_window> " << endl;
		exit(0);

	}else if( argc == 3){
		port = atoi(argv[1]);
		recv_window = atoi(argv[2]);
	}

	ServerSockUDP* server_udp = new ServerSockUDP(AF_INET, SOCK_DGRAM , 0, port);
	bool flag = false;
	client_length = sizeof(client_details);
	while(1){

		// recv the request from the client.
		recvfrom(server_udp->get_socket(),buffer,MTU,0,(struct sockaddr *)&client_details,&client_length);
		// Recieve the buffer, parse request, send response
		RudpSegment* segment = parse_request(buffer);

		if(segment->data && segment->ack_flag == 'N'){
			ifstream req_file;
			req_file.open((char *)segment->data);

			if(!req_file.is_open()){
				// Unable to open file send back fin.
				cout << "Error in opening file" << endl;
				exit(2);
			} else{
				req_file.close();
				// Can open the file, segment and send it on socket.
				// Send file name for that to process.
				if(send_segments(segment->data,server_udp->get_socket(),client_details,recv_window)) {
					// Wait for 500 ms before closing connection.
					flag = true;
				}else {
					cout << "Unable to send all the data" << endl;
					exit(1);
				}
			}
		}

		if(flag){
			cout << "Sent all segments. Closing server connection." << endl;
			break;
		}
	}

	close(server_udp->get_socket());

	return 0;

}