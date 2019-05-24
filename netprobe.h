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

typedef struct netsock_queue_node{
	netsock *ns;
	struct netsock_queue_node *next;
} NSNode;

typedef struct netsock_queue_head{
	NSNode *front;
	NSNode *rear;
	int num;
} NSQueue;

/*typedef struct thread_monitor{
	int threadnum;
	pthread_t *sockt;
	NSQueue *queue;
} Thread_monitor;*/



//char *request_encode(struct Netprobe np);

//struct Netprobe *request_decode(char *buffer);