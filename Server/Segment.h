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

class Segment{
public:
	int file_length;
	ifstream ifs;
	byte4 init_seq_no;
	RudpSegment* rudpSegment;
	Segment(byte4 init_seq_no);
	~Segment();
	RudpSegment* get_Rudp(byte4 seq_no);
	byte* get_segment(byte4 seq_no);
	void open_file(byte* filename);
};