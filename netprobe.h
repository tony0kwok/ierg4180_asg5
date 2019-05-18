struct Netprobe{
	int mode;
	int proto;
};

typedef struct send_set{
	long bsize;
	long pktrate;
	long num;
	long sbufsize;
	long sent;
} SendSet;

typedef struct recv_set{
	long bsize;
	long rbufsize;
	long received;
} RecvSet;

typedef struct netsock{
	int sockfd;
	struct sockaddr_in clientInfo;
	socklen_t addrlen;
	struct Netprobe *np;
	SendSet *s_set;
	RecvSet *r_set;
	ES_Timer timer;
} Netsock;

//char *request_encode(struct Netprobe np);

//struct Netprobe *request_decode(char *buffer);