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
#include "RudpSegment.h"

using namespace std;

#define MTU 1472
#define HDR_LEN 12
#define TRUE 'Y'
#define FALSE 'N'

typedef unsigned char byte;
typedef unsigned int byte4;
typedef unsigned short byte2;

RudpSegment::RudpSegment(byte4 seq_number,byte fin_flag,byte* data,byte4 ack_number,byte ack_flag){
	this->sequence_number = seq_number;
	this->ack_number = ack_number;
	this->ack_flag = ack_flag;
	this->fin_flag = fin_flag;
	this->length = strlen((char *)data);
	this->data = data;
}

RudpSegment::RudpSegment(){ };

byte* RudpSegment::prepare_segment(){

	int counter = 0;
	byte* buffer = (byte *) calloc(MTU,sizeof(byte));
	
	memset(buffer,0,MTU);

	memcpy(buffer+counter,&sequence_number,sizeof(sequence_number));
	counter += sizeof(sequence_number);
	
	memcpy(buffer+counter,&ack_number,sizeof(ack_number));
	counter += sizeof(ack_number);
	
	memcpy(buffer+counter,&ack_flag,1);
	counter += sizeof(ack_flag);
	
	memcpy(buffer+counter,&fin_flag,1);
	counter += sizeof(fin_flag);

	memcpy(buffer+counter,&length,sizeof(length));
	counter += sizeof(length);

	memcpy(buffer+counter,data,length);

	return buffer;
}
