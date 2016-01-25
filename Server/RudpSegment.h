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

using namespace std;

#define MTU 1472
#define HDR_LEN 12
#define TRUE 'Y'
#define FALSE 'N'

typedef unsigned char byte;
typedef unsigned int byte4;
typedef unsigned short byte2;

class RudpSegment {
public:
	byte4 sequence_number;
	byte4 ack_number;
	byte ack_flag;
	byte fin_flag;
	byte2 length;
	byte* data;	// converted to char* at the time of sending. size = 1460
	RudpSegment(byte4 seq_number,byte fin_flag,byte* data,byte4 ack_number=0,byte ack_flag=FALSE);
	RudpSegment();
	byte* prepare_segment();
};