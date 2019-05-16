struct Netprobe{
	int mode;
	int proto;
	long bsize;
};

struct send_set{
	int mode;
	char hostname[100];
	int port;
	int proto;
	long bsize;
	long pktrate;
	long num;
	long sbufsize;
} SendSet;

struct recv_set{
	int port;
	int proto;
	long bsize;
	long rbufsize;
} RecvSet;

char *request_encode(struct Netprobe np);

struct Netprobe *request_decode(char *buffer);