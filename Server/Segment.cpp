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
#include "Segment.h"

using namespace std;

#define MTU 1472
#define HDR_LEN 12
#define TRUE 'Y'
#define FALSE 'N'

typedef unsigned char byte;
typedef unsigned int byte4;
typedef unsigned short byte2;

void Segment::open_file(byte* filename){
	this->ifs.open((char *)filename);
	this->ifs.seekg(0,ios::end);
	this->file_length = ifs.tellg();
	this->ifs.seekg(0,ios::beg);
}

Segment::Segment(byte4 init_seq_no){
	this->init_seq_no = init_seq_no;
}

Segment::~Segment(){
	this->ifs.close();
}

byte* get_send_buffer(byte4 seq_no,byte fin_flag,byte* data){
	RudpSegment* sender = new RudpSegment(seq_no,fin_flag,data);
	return sender->prepare_segment();
}

RudpSegment* Segment::get_Rudp(byte4 seq_no){
	byte* data = (byte *) calloc(MTU - HDR_LEN,sizeof(byte));;
	memset(data,0,(MTU-HDR_LEN));

	ifs.seekg((seq_no-init_seq_no),ios::beg);

	if(file_length-(seq_no-init_seq_no) < MTU-HDR_LEN){
		ifs.read((char *)data,(file_length-(seq_no-init_seq_no)));
		return new RudpSegment(seq_no,TRUE,data);
	}
	ifs.read((char *)data,(MTU-HDR_LEN));

	return new RudpSegment(seq_no,FALSE,data);
}

byte* Segment::get_segment(byte4 seq_no){
	return get_Rudp(seq_no)->prepare_segment();
}