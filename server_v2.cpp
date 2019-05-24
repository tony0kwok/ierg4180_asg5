#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>   
#include <netdb.h>   
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include "es_timer.h"
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <errno.h>
#include <sys/errno.h>

#include "netprobe.h"
#include "tinythread.h"

using namespace tthread;

#define SEND 1
#define RECV 0

void errExit(char *reason) {
    char *buff = reason ? reason : strerror(errno);
    printf("Error: %s", buff);
    exit(EXIT_FAILURE);
}

//NetProbe functions
char *request_encode(struct Netprobe np){
	char *buffer=(char*)malloc(sizeof(np));
	memcpy(buffer,(const char*) &np,sizeof(np));
	return buffer;
}

struct Netprobe *request_decode(char *buffer){
	struct Netprobe *np = (struct Netprobe*)malloc(sizeof(struct Netprobe));
	memcpy(np,(struct Netprobe*)buffer,sizeof(struct Netprobe));
	return np;
}

SendSet sendset_encode(char *buffer, SendSet s_set){
	memcpy(buffer,(const char*) &s_set,sizeof(s_set));
	return s_set;
}

RecvSet recvset_encode(char *buffer, RecvSet r_set){
	memcpy(buffer,(const char*) &r_set,sizeof(r_set));
	return r_set;
}

SendSet *sendset_decode(char *buffer){
	SendSet *s_set = (SendSet*)malloc(sizeof(SendSet));
	memcpy(s_set,(SendSet*)buffer,sizeof(SendSet));
	return s_set;
}

RecvSet *recvset_decode(char *buffer){
	RecvSet *r_set = (RecvSet*)malloc(sizeof(RecvSet));
	memcpy(r_set,(RecvSet*)buffer,sizeof(RecvSet));
	return r_set;
}

void showNetprobe(struct Netprobe *np){
	printf("Netprobe->mode: %d\n", np->mode);
	printf("Netprobe->proto: %d\n", np->proto);
}

void showSendSet(SendSet *s_set){
	printf("SendSet->bsize: %ld\n", s_set->bsize);
	printf("SendSet->pktrate: %ld\n", s_set->pktrate);
	printf("SendSet->num: %ld\n", s_set->num);
	printf("SendSet->sbufsize: %ld\n", s_set->sbufsize);
	printf("SendSet->sent: %ld\n", s_set->sent);
}

void showRecvSet(RecvSet *r_set){
	printf("RecvSet->bsize: %ld\n", r_set->bsize);
	printf("RecvSet->rbufsize: %ld\n", r_set->rbufsize);
	printf("RecvSet->received: %ld\n", r_set->received);
}

void closeNetsock(Netsock *ns){
	ns->sockfd = 0;
	free(ns->s_set);
	free(ns->r_set);
	free(ns->np);
}

void enqueue(NSQueue *queue, Netsock *ns){
	//printf("start enqueue\n");
	NSNode *node = (NSNode*)malloc(sizeof(NSNode));
	node->ns = ns;
	node->next = NULL;
	
	if (queue->num==0)
	{
		queue->front = node;
	}
	else
	{
		queue->rear->next = node;
	}
	queue->rear = node;
	queue->num += 1;
	//printf("end enqueue\n");
}

int dequeue(NSQueue *queue, Netsock **nspointer){
	if (queue->num==0)
	{
		return 0;
	}
	//printf("start dequeue\n");
	*nspointer = queue->front->ns;
	if (queue->num==1)
	{
		free(queue->front);
		queue->front = NULL;
		queue->rear = NULL;
	}
	else
	{
		queue->front = queue->front->next;
	}
	queue->num -= 1;
	//printf("end dequeue\n");
	return 1;
}

//=======================================================
pthread_mutex_t queueMutex;
pthread_mutex_t numOfTakenMutex;
int taken_thread_tcp;
int taken_thread_udp;
int threadnum;

int mode;
int protocol;
int domain;
char address[40];
int port;
int stat_ms;
char stat[200];
long rbufsize;
long sbufsize;
char hostname[100];

char *port_num;

int inet_type;

void encode_header(char *buffer, int number){
	//support maximum sequence number 10000 packages
	sprintf(buffer, "%d%d%d%d%d%d%d%d", number/10000000, (number%10000000)/1000000, (number%1000000)/100000, (number%100000)/10000, (number%10000)/1000, (number%1000)/100, (number%100)/10, number%10);
}	

long decode_header(char *buffer){
	return atoll(buffer);
}

int setting(int argc, char** argv){
	//default setting
	protocol = 0;
	memset(address, '\0', sizeof(address));
	char tony[] = "127.0.0.1";
	strcpy(address,tony);
	port = 4180;
	port_num = (char *)malloc(100);
	strcpy(port_num, "4180");

	stat_ms = 500;
	memset(stat, '\0', sizeof(stat));
	strcpy(stat, "TCP Clients [0], UDP Clients [0]\n");

	rbufsize = 1000;
	sbufsize = 1000;

	strcpy(hostname, "localhost");

	taken_thread_tcp = 0;
	taken_thread_udp = 0;
	threadnum = 8;

	mode = 4;	//3 = select, 4 = thread

	inet_type = AF_UNSPEC;

	const char *optstring = "m:p:u:f:l:s:o:";
    int c;
    struct option opts[] = {
    	{"ipv4", 0, NULL, '4'},
    	{"ipv6", 0, NULL, '6'},
        {"stat", 1, NULL, 'm'},
        {"lport", 1, NULL, 'p'},
        {"sbufsize", 1, NULL, 'u'},
        {"rbufsize", 1, NULL, 'f'},
        {"lhost", 1, NULL, 'l'},
        {"servermodel", 1, NULL, 's'},
        {"poolsize", 1, NULL, 'o'}
    };
    while((c = getopt_long_only(argc, argv, optstring, opts, NULL)) != -1) {
        switch(c) {
        	case '4':
        		inet_type = AF_INET;
        		break;
        	case '6':
        		inet_type = AF_INET6;
        		break;
        	case 'o':
        		threadnum = atoi(optarg);
        	case 's':
        		if (strcmp(optarg, "select")==0)
        		{
        			mode = 3;
        		}
        		break;
        	case 'l':
        		strcpy(hostname, optarg);
        		break;
        	case 'f':
        		rbufsize = atol(optarg);
        		break;
        	case 'u':
        		sbufsize = atol(optarg);
        		break;
            case 'p':
                port = atol(optarg);
                strcpy(port_num, optarg);
                break;
            case 'm':
            	stat_ms = atol(optarg);
                break;
            case '?':
                printf("unknown option\n");
                break;
            case 0 :
                printf("the return val is 0\n");
                break;
            default:
                printf("------\n");
        }
    }
}

void msleep(int ms){
	usleep(1000*ms);
	return;
}

/*char *send_stat_cal(int usec_time, int package_no){
	char *output = (char *) malloc(100);
	sprintf(output, "Rate[%.2lfMbps]", (double)(package_no+1)*bsize*1000/usec_time);
	return output;
}

char *recv_stat_cal(long usec_time, int package_no){
	static long recieved = 0, max = 0;
	static long previous_time = 0, pretrans_time = 0;
	static long deltasum = 0;
	static long lost = 0;
	double jitter = 0;
	if (package_no>max)
		max = package_no;
	recieved++;
	lost = max+1 - recieved;
	double lostrate = (double)lost/recieved;
	double rate = (double)bsize*recieved*1000/usec_time;
	
	if (recieved>=2){
		if (int delta = usec_time - previous_time - pretrans_time>0)
			deltasum += delta;
		else
			deltasum -= delta;
	}
	jitter = deltasum/(float)recieved;
	pretrans_time = usec_time - previous_time;
	previous_time = usec_time;

	char *output = (char *)malloc(100);
	sprintf(output, "Pkts [%lld] Lost [%lld, %.2lf%%] Rate [%.2lfMbps] Jitter [%.2lfus]", recieved, lost, lostrate, rate, jitter);
	return output;
}*/

void* threadfunc(void* data){
	ES_Timer timer;
	timer.Start();
	char *elapsedtime = (char *)malloc(100);
	while(1){
		sprintf(elapsedtime, "Elapsed [%.2lfs] ",(double)(timer.ElapseduSec())/1000000);
		msleep(stat_ms);
		strcat(elapsedtime, stat);
		printf("%s\n", elapsedtime);
	}
	return NULL;
}

void *sockThread(void *arg){
	ES_Timer reuse_timer;
	while(1){
		sprintf(stat, "ThreadPool [%d|%d] TCP Clients [%d] UDP Clients [%d]", threadnum, taken_thread_tcp+taken_thread_udp, taken_thread_tcp, taken_thread_udp);
		pthread_mutex_lock(&queueMutex);
		Netsock *ns;
		while(1){
			if(dequeue((NSQueue *)arg, &ns))	//return false when the queue is empty
			{
				//showNetprobe(ns->np);
				if (ns->np->proto==SOCK_STREAM)
				{
					pthread_mutex_lock(&numOfTakenMutex);
					taken_thread_tcp++;
					pthread_mutex_unlock(&numOfTakenMutex);
				}
				else
				if (ns->np->proto==SOCK_DGRAM)
				{
					pthread_mutex_lock(&numOfTakenMutex);
					taken_thread_udp++;
					pthread_mutex_unlock(&numOfTakenMutex);
				}
				sprintf(stat, "ThreadPool [%d|%d] TCP Clients [%d] UDP Clients [%d]", threadnum, taken_thread_tcp+taken_thread_udp, taken_thread_tcp, taken_thread_udp);
				break;
			}
			else
			{
				//printf("the queue is empty\n");
			}
		}
		pthread_mutex_unlock(&queueMutex);

		char *sendBuffer = (char *) malloc(sizeof(char)*sbufsize);
		char *recvBuffer = (char *) malloc(sizeof(char)*rbufsize);
		memset(sendBuffer, '\0', sbufsize);
		memset(recvBuffer, '\0', rbufsize);

		int result;

		//do receiving
		if(ns->np->mode==RECV){
			//apply to both tcp and udp
			if(result = recv(ns->sockfd,recvBuffer,rbufsize,0)>0){
				//printf("recving tcp message from %s\n, fd %d\n", inet_ntoa(ns->clientInfo.sin_addr), ns->sockfd);
			}
			if (result<=0)
			{
				closeNetsock(ns);
				if (ns->np->proto==SOCK_STREAM)
				{
					pthread_mutex_lock(&numOfTakenMutex);
					taken_thread_tcp--;
					pthread_mutex_unlock(&numOfTakenMutex);
				}
				else
				if (ns->np->proto==SOCK_DGRAM)
				{
					pthread_mutex_lock(&numOfTakenMutex);
					taken_thread_udp--;
					pthread_mutex_unlock(&numOfTakenMutex);
				}
				
				continue;
			}
		}
		else
		if (ns->np->mode==SEND)
		{
			int run = 1;
			if(ns->s_set->sent<ns->s_set->num){
				if (ns->np->proto==SOCK_STREAM)
				{
					//if number of socket sent is less than it should be
					/*printf("num of package should be sent%ld\n", (long)(((double)ns->timer.Elapsed())/ns->s_set->bsize*ns->s_set->pktrate/1000));
					printf("sent %ld package\n", ns->s_set->sent);
					printf("time :%lds\n", ns->timer.Elapsed()/1000);*/
					if (ns->s_set->pktrate==0)
			    	{
				    	//need to improve, because it send one whole package in a loop
						long temsum = 0;
						long sendb;
				    	encode_header(sendBuffer, ns->s_set->sent);
				    	

				    	while(run){
					    	//keep sending before put all data of a package into buf
					    	while(ns->s_set->bsize>temsum){
						    	sendb = send(ns->sockfd, sendBuffer+temsum, ns->s_set->sbufsize > ns->s_set->bsize-temsum ? ns->s_set->bsize - temsum: ns->s_set->sbufsize, 0);
						    	if (sendb<=0)
						    	{
						    		//perror("error: ");
						    		closeNetsock(ns);
						    		run = 0;
						    		pthread_mutex_lock(&numOfTakenMutex);
									taken_thread_tcp--;
									pthread_mutex_unlock(&numOfTakenMutex);
						    		break;
						    	}
						    	temsum += sendb;
						    	//printf("I have sent %ld bytes\n", sendb);
						    }

						    ns->s_set->sent += 1;
						    //==============================
						}
						continue;
			    	}
				    else
				    while(run)
				    {
						if (ns->s_set->sent < (long)(((double)ns->timer.Elapsed())/ns->s_set->bsize*ns->s_set->pktrate/1000))
						{
							//need to improve, because it send one whole package in a loop
							long temsum = 0;
							long sendb;
					    	encode_header(sendBuffer, ns->s_set->sent);

					    	//keep sending before put all data of a package into buf
					    	while(ns->s_set->bsize>temsum){
						    	sendb = send(ns->sockfd, sendBuffer+temsum, ns->s_set->sbufsize > ns->s_set->bsize-temsum ? ns->s_set->bsize - temsum: ns->s_set->sbufsize, 0);
						    	if (sendb<=0)
						    	{
						    		//perror("error: ");
						    		closeNetsock(ns);
						    		run = 0;
						    		pthread_mutex_lock(&numOfTakenMutex);
									taken_thread_tcp--;
									pthread_mutex_unlock(&numOfTakenMutex);
						    		break;
						    	}
						    	//printf("I have sent %ld bytes\n", sendb);
						    	temsum += sendb;
						    }

						    ns->s_set->sent += 1;
						    //==============================
						}
					}	
				}
				else
				if (ns->np->proto==SOCK_DGRAM)
				{
					/*printf("num of package should be sent%ld\n", (long)(((double)ns->timer.Elapsed())/ns->s_set->bsize*ns->s_set->pktrate/1000));
					printf("sent %ld package\n", ns->s_set->sent);
					printf("time :%lds\n", ns->timer.Elapsed()/1000);*/
					if (ns->s_set->pktrate==0)
					{
						//need to improve
				    	long temsum = 0;
				    	long sendb;
				    	encode_header(sendBuffer, ns->s_set->sent);

				    	while(run){
				    	//keep sending before put all data of a package into buf
					    	while(ns->s_set->bsize>temsum){
						    	sendb = sendto(ns->sockfd, sendBuffer+temsum, ns->s_set->sbufsize > ns->s_set->bsize-temsum ? ns->s_set->bsize-temsum: ns->s_set->sbufsize, 0, (struct sockaddr *)(&(ns->clientInfo)),sizeof(ns->clientInfo));
						    	if (sendb<=0)
						    	{
						    		//perror("error: ");
						    		closeNetsock(ns);
						    		run = 0;
						    		pthread_mutex_lock(&numOfTakenMutex);
									taken_thread_udp--;
									pthread_mutex_unlock(&numOfTakenMutex);
						    		break;
						    	}
						    	temsum += sendb;
						    }

					    	ns->s_set->sent += 1;
					    	//===============
					    }
					}
					else
					while(run){
						if (ns->s_set->sent < (long)(((double)ns->timer.Elapsed())/ns->s_set->bsize*ns->s_set->pktrate/1000))
						{
							//need to improve
					    	long temsum = 0;
					    	long sendb;
					    	encode_header(sendBuffer, ns->s_set->sent);

					    	//keep sending before put all data of a package into buf
					    	while(ns->s_set->bsize>temsum){
						    	sendb = sendto(ns->sockfd, sendBuffer+temsum, ns->s_set->sbufsize > ns->s_set->bsize-temsum ? ns->s_set->bsize-temsum: ns->s_set->sbufsize, 0, (struct sockaddr *)(&(ns->clientInfo)),sizeof(ns->clientInfo));
						    	if (sendb<=0)
						    	{
						    		//perror("error: ");
						    		closeNetsock(ns);
						    		run = 0;
						    		pthread_mutex_lock(&numOfTakenMutex);
									taken_thread_udp--;
									pthread_mutex_unlock(&numOfTakenMutex);
						    		break;
						    	}
						    	temsum += sendb;
						    }

						    ns->s_set->sent += 1;
						    //===============
						}
					}
				}
			}
			else{
				closeNetsock(ns);
				if (ns->np->proto==SOCK_STREAM)
				{
					pthread_mutex_lock(&numOfTakenMutex);
					taken_thread_tcp--;
					pthread_mutex_unlock(&numOfTakenMutex);
				}
				else
				if (ns->np->proto==SOCK_DGRAM)
				{
					pthread_mutex_lock(&numOfTakenMutex);
					taken_thread_udp--;
					pthread_mutex_unlock(&numOfTakenMutex);
				}
				run = 0;
			}
		
		}
	}


	msleep(1000);
	return 0;
}

void threadServer(){
	//set up the netsock queue
	NSQueue queue = {NULL, NULL, 0};

	pthread_t *sockt = (pthread_t *)malloc(threadnum*sizeof(pthread_t));

	for (int i = 0; i < threadnum; i++)
	{
		pthread_create(sockt+i, NULL, sockThread, (void *)&queue);
	}

	//start main thread
	SendSet sendset[100];
	RecvSet recvset[100];

	Netsock ns[200];
	int netsockmax = 0;

	//set up the netsock
	for (int i = 0; i < 200; i++)
	{
		(ns+i)->addrlen = sizeof((ns+i)->clientInfo);
    	bzero(&((ns+i)->clientInfo),sizeof((ns+i)->clientInfo));
	}
	//==========================

	char *sendBuffer = (char *) malloc(sizeof(char)*sbufsize);
	char *recvBuffer = (char *) malloc(sizeof(char)*rbufsize);
	memset(sendBuffer, '\0', sbufsize);
	memset(recvBuffer, '\0', rbufsize);
    
    //max number of tcp client
    int tcpClientSockfd[100];
    int tcpClientMode[100];
    int tcpmax = 0;

    //udp client
    struct sockaddr_in udpClientInfo[100];
    for (int i = 0; i < 100; ++i)
    {
    	bzero(udpClientInfo+i,sizeof(struct sockaddr_in));
    }
    int udpClientMode[100];
    int udpmax = 0;

    //for checking available tcp and udp socket
    int available_tcp = 0;
    int available_udp = 0;

	//SOCKET SETUP
	int listen_sockfd = 0, udpsockfd = 0;

	//get ipv6 address
	int gaiStatus; // getaddrinfo 狀態碼
    struct addrinfo hints; // hints 參數，設定 getaddrinfo() 的回傳方式
    struct addrinfo *result, *udp_result; // getaddrinfo() 執行結果的 addrinfo 結構指標

    // 以 memset 清空 hints 結構
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = inet_type; // 使用 IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // 串流 Socket
    hints.ai_flags = AI_NUMERICSERV; // 將 getaddrinfo() 第 2 參數 (PORT_NUM) 視為數字

    // 以 getaddrinfo 透過 DNS，取得 addrinfo 鏈結串列 (Linked List)
    // 以從中取得 Host 的 IP 位址
    if ((gaiStatus = getaddrinfo(strcmp(hostname, "localhost")==0?NULL:hostname, port_num, &hints, &result)) != 0)
        errExit((char *) gai_strerror(gaiStatus));

    //get udp address
    hints.ai_socktype = SOCK_DGRAM;
    if ((gaiStatus = getaddrinfo(strcmp(hostname, "localhost")==0?NULL:hostname, port_num, &hints, &udp_result)) != 0)
        errExit((char *) gai_strerror(gaiStatus));

	listen_sockfd = socket(result->ai_family, result->ai_socktype, 0);
    udpsockfd = socket(udp_result->ai_family, udp_result->ai_socktype, 0);

    if (listen_sockfd == -1 || udpsockfd == -1){
        printf("Fail to create a socket.");
        exit(1);
    }

	//sever socket的連線
    struct sockaddr_in serverInfo,clientInfo;
    socklen_t addrlen = sizeof(clientInfo);
    bzero(&serverInfo,sizeof(serverInfo));
    bzero(&clientInfo,sizeof(clientInfo));

    serverInfo.sin_family = PF_INET;
    struct hostent *he;
    if ( (he = gethostbyname(hostname) ) == NULL ) {
    	printf("Invalid hostname\n");
      	exit(1); /* error */
  	}
    memcpy(&serverInfo.sin_addr, he->h_addr_list[0], he->h_length);
    //serverInfo.sin_addr.s_addr = INADDR_ANY;
    serverInfo.sin_port = htons(port);
    bind(listen_sockfd,result->ai_addr, result->ai_addrlen);
    bind(udpsockfd,udp_result->ai_addr, udp_result->ai_addrlen);
    listen(listen_sockfd, 5);

   	//select set up
   	int max = 0;
   	
   	while(1){
   		tcpClientSockfd[tcpmax] = accept(listen_sockfd,(struct sockaddr *)(&(ns[netsockmax].clientInfo)), &(ns[netsockmax].addrlen));
   		printf("TCP connection established\n");
   		if ((taken_thread_tcp+taken_thread_udp+1)>=threadnum)
   		{
   			pthread_t *sockt = (pthread_t *)malloc(threadnum*sizeof(pthread_t));

			for (int i = 0; i < threadnum; i++)
			{
				pthread_create(sockt+i, NULL, sockThread, (void *)&queue);
			}

			threadnum *= 2;
   		}

		if (recv(tcpClientSockfd[tcpmax], recvBuffer, rbufsize, 0)>0)
		{
			//set the ns
			ns[netsockmax].np = request_decode(recvBuffer);
			//=====================================

			if (ns[netsockmax].np->mode==RECV)
				{
					ns[netsockmax].r_set = recvset_decode(recvBuffer+sizeof(struct Netprobe));
				}
			if (ns[netsockmax].np->mode==SEND)
				{
					ns[netsockmax].s_set = sendset_decode(recvBuffer+sizeof(struct Netprobe));
				}

			if(ns[netsockmax].np->proto==SOCK_STREAM){
				ns[netsockmax].sockfd = tcpClientSockfd[tcpmax];
				tcpmax++;
				available_tcp++;
			}
			if(ns[netsockmax].np->proto==SOCK_DGRAM){
				//close(tcpClientSockfd[tcpmax]);

				ns[netsockmax].sockfd = udpsockfd;
				udpClientInfo[udpmax] = clientInfo;
				udpmax++;
				available_udp++;

				//we sleep for 1 second because I found it is too fast for client to respond when pktrate is 0
				msleep(1000);
			}
			ns[netsockmax].timer.Start();
			

			//insert ns into queue
			enqueue(&queue, ns+netsockmax);

			netsockmax++;
		}
   	}
}

void selectServer(){
	SendSet sendset[100];
	RecvSet recvset[100];

	Netsock ns[200];
	int netsockmax = 0;

	//set up the netsock
	for (int i = 0; i < 200; i++)
	{
		(ns+i)->addrlen = sizeof((ns+i)->clientInfo);
    	bzero(&((ns+i)->clientInfo),sizeof((ns+i)->clientInfo));
	}
	//==========================

	char *sendBuffer = (char *) malloc(sizeof(char)*sbufsize);
	char *recvBuffer = (char *) malloc(sizeof(char)*rbufsize);
	memset(sendBuffer, '\0', sbufsize);
	memset(recvBuffer, '\0', rbufsize);
    
    //max number of tcp client
    int tcpClientSockfd[100];
    int tcpClientMode[100];
    int tcpmax = 0;

    //udp client
    struct sockaddr_in udpClientInfo[100];
    for (int i = 0; i < 100; ++i)
    {
    	bzero(udpClientInfo+i,sizeof(struct sockaddr_in));
    }
    int udpClientMode[100];
    int udpmax = 0;

    //for checking available tcp and udp socket
    int available_tcp = 0;
    int available_udp = 0;

	//SOCKET SETUP
	int listen_sockfd = 0, udpsockfd = 0;

	listen_sockfd = socket(AF_INET , SOCK_STREAM , 0);
    udpsockfd = socket(AF_INET , SOCK_DGRAM , 0);

    if (listen_sockfd == -1 || udpsockfd == -1){
        printf("Fail to create a socket.");
        exit(1);
    }

	//sever socket的連線
    struct sockaddr_in serverInfo,clientInfo;
    socklen_t addrlen = sizeof(clientInfo);
    bzero(&serverInfo,sizeof(serverInfo));
    bzero(&clientInfo,sizeof(clientInfo));

    serverInfo.sin_family = PF_INET;
    struct hostent *he;
    if ( (he = gethostbyname(hostname) ) == NULL ) {
    	printf("Invalid hostname\n");
      	exit(1); /* error */
  	}
    memcpy(&serverInfo.sin_addr, he->h_addr_list[0], he->h_length);
    //serverInfo.sin_addr.s_addr = INADDR_ANY;
    serverInfo.sin_port = htons(port);
    bind(listen_sockfd,(struct sockaddr *)&serverInfo,sizeof(serverInfo));
    bind(udpsockfd,(struct sockaddr *)&serverInfo,sizeof(serverInfo));
    listen(listen_sockfd, 5);

   	//select set up
   	int max = 0;
   	fd_set read_fds, write_fds;

    while(1){
    	//set up the time out
	    struct timeval timeout; 
	    int result;

	    FD_ZERO(&read_fds);
	    FD_ZERO(&write_fds);
	    FD_SET(listen_sockfd,&read_fds);
	    if (listen_sockfd>max)
		   max = listen_sockfd;
	    /*FD_SET(udpsockfd,&read_fds);
	    if (udpsockfd>max)
	    	max = udpsockfd;*/

	    //add all sockfd in ns array
	    for (int i = 0; i < netsockmax; i++)
	    {
	    	if (ns[i].sockfd==0)
	    	{
	    		continue;
	    	}
	    	if (ns[i].np->mode==SEND)
	    		FD_SET(ns[i].sockfd, &write_fds);
	    	else
	    	if (ns[i].np->mode==RECV)
	    		FD_SET(ns[i].sockfd, &read_fds);
	    	
	    	if (ns[i].sockfd>max)
	    	{
	    		max=ns[i].sockfd;
	    	}
	    }

	    //add all connected tcp into fd set
	    /*for(int i=0;i<tcpmax;i++){
	    	if (tcpClientSockfd[i]==0)	//means Sockfd expired
	    		continue;
		    FD_SET(tcpClientSockfd[i],&read_fds);
		    if (tcpClientSockfd[i]>max)
		    	max = tcpClientSockfd[i];
		}*/

		if(result = select(max+1, &read_fds, &write_fds, NULL, NULL)<0){
			perror("select error: ");
			exit(1);
		}

		for (int i = 0; i < netsockmax; i++)
		{
			int result;
			//do receiving
			if(ns[i].np->mode==RECV){
				if(FD_ISSET(ns[i].sockfd, &read_fds)){
					//apply to both tcp and udp
					if(result = recv(ns[i].sockfd,recvBuffer,rbufsize,0)>0){
						//printf("recving tcp message from %s\n, fd %d\n", inet_ntoa(ns[i].clientInfo.sin_addr), ns[i].sockfd);
					}
					if (result<=0)
					{
						closeNetsock(ns+i);
						if (ns[i].np->proto==SOCK_STREAM)
							available_tcp--;
						else
						if (ns[i].np->proto==SOCK_DGRAM)
							available_udp--;
					}
				}
			}
			else
			if (ns[i].np->mode==SEND)
			{
				if (FD_ISSET(ns[i].sockfd, &write_fds))
				{
					if(ns[i].s_set->sent<ns[i].s_set->num){
						if (ns[i].np->proto==SOCK_STREAM)
						{
							//if number of socket sent is less than it should be
							/*printf("num of package should be sent%ld\n", (long)(((double)ns[i].timer.Elapsed())/ns[i].s_set->bsize*ns[i].s_set->pktrate/1000));
							printf("sent %ld package\n", ns[i].s_set->sent);
							printf("time :%lds\n", ns[i].timer.Elapsed()/1000);*/
							if (ns[i].s_set->pktrate==0)
					    	{
						    	//need to improve, because it send one whole package in a loop
								long temsum = 0;
								long sendb;
						    	encode_header(sendBuffer, ns[i].s_set->sent);

						    	//keep sending before put all data of a package into buf
						    	while(ns[i].s_set->bsize>temsum){
							    	sendb = send(ns[i].sockfd, sendBuffer+temsum, ns[i].s_set->sbufsize > ns[i].s_set->bsize-temsum ? ns[i].s_set->bsize - temsum: ns[i].s_set->sbufsize, 0);
							    	if (sendb<=0)
							    	{
							    		perror("error: ");
							    		closeNetsock(ns+i);
							    		available_tcp--;
							    	}
							    	temsum += sendb;
							    }

							    ns[i].s_set->sent += 1;
							    //==============================
					    	}
						    else
							if (ns[i].s_set->sent < (long)(((double)ns[i].timer.Elapsed())/ns[i].s_set->bsize*ns[i].s_set->pktrate/1000))
							{
								//need to improve, because it send one whole package in a loop
								long temsum = 0;
								long sendb;
						    	encode_header(sendBuffer, ns[i].s_set->sent);

						    	//keep sending before put all data of a package into buf
						    	while(ns[i].s_set->bsize>temsum){
							    	sendb = send(ns[i].sockfd, sendBuffer+temsum, ns[i].s_set->sbufsize > ns[i].s_set->bsize-temsum ? ns[i].s_set->bsize - temsum: ns[i].s_set->sbufsize, 0);
							    	if (sendb<=0)
							    	{
							    		perror("error: ");
							    		closeNetsock(ns+i);
							    		available_tcp--;
							    	}
							    	temsum += sendb;
							    }

							    ns[i].s_set->sent += 1;
							    //==============================
							}
						}
						else
						if (ns[i].np->proto==SOCK_DGRAM)
						{
							/*printf("num of package should be sent%ld\n", (long)(((double)ns[i].timer.Elapsed())/ns[i].s_set->bsize*ns[i].s_set->pktrate/1000));
							printf("sent %ld package\n", ns[i].s_set->sent);
							printf("time :%lds\n", ns[i].timer.Elapsed()/1000);*/
							if (ns[i].s_set->pktrate==0)
							{
								//need to improve
						    	long temsum = 0;
						    	long sendb;
						    	encode_header(sendBuffer, ns[i].s_set->sent);

						    	//keep sending before put all data of a package into buf
						    	while(ns[i].s_set->bsize>temsum){
							    	sendb = sendto(ns[i].sockfd, sendBuffer+temsum, ns[i].s_set->sbufsize > ns[i].s_set->bsize-temsum ? ns[i].s_set->bsize-temsum: ns[i].s_set->sbufsize, 0, (struct sockaddr *)(&(ns[i].clientInfo)),sizeof(ns[i].clientInfo));
							    	if (sendb<=0)
							    	{
							    		perror("error: ");
							    		closeNetsock(ns+i);
							    		available_udp--;
							    	}
							    	temsum += sendb;
							    }

							    ns[i].s_set->sent += 1;
							    //===============
							}
							if (ns[i].s_set->sent < (long)(((double)ns[i].timer.Elapsed())/ns[i].s_set->bsize*ns[i].s_set->pktrate/1000))
							{
								//need to improve
						    	long temsum = 0;
						    	long sendb;
						    	encode_header(sendBuffer, ns[i].s_set->sent);

						    	//keep sending before put all data of a package into buf
						    	while(ns[i].s_set->bsize>temsum){
							    	sendb = sendto(ns[i].sockfd, sendBuffer+temsum, ns[i].s_set->sbufsize > ns[i].s_set->bsize-temsum ? ns[i].s_set->bsize-temsum: ns[i].s_set->sbufsize, 0, (struct sockaddr *)(&(ns[i].clientInfo)),sizeof(ns[i].clientInfo));
							    	if (sendb<=0)
							    	{
							    		perror("error: ");
							    		closeNetsock(ns+i);
							    		available_udp--;
							    	}
							    	temsum += sendb;
							    }

							    ns[i].s_set->sent += 1;
							    //===============
							}
						}
					}
					else{
						closeNetsock(ns+i);
						if (ns[i].np->proto==SOCK_STREAM)
							available_tcp--;
						else
						if (ns[i].np->proto==SOCK_DGRAM)
							available_udp--;
					}
				}
			}
		}

		//receive udp request

		/*if (FD_ISSET(udpsockfd, &read_fds))
		{
			recvfrom(udpsockfd, recvBuffer, sizeof(recvBuffer), 0, (struct sockaddr*)&clientInfo, &addrlen);
			char *ip = inet_ntoa(clientInfo.sin_addr);
			printf("recving udp message from %s\n", ip);
		}*/

		/*for (int i = 0; i < udpmax; i++)
		{
			if(FD_ISSET(tcpClientSockfd[i], &read_fds)){
				recv(tcpClientSockfd[],recvBuffer,10000,0);
			}	
		}*/

		if (FD_ISSET(listen_sockfd, &read_fds))
		{
			tcpClientSockfd[tcpmax] = accept(listen_sockfd,(struct sockaddr *)(&(ns[netsockmax].clientInfo)), &(ns[netsockmax].addrlen));
			if (recv(tcpClientSockfd[tcpmax], recvBuffer, rbufsize, 0)>0)
			{
				//set the ns
				ns[netsockmax].np = request_decode(recvBuffer);
				//=====================================

				if (ns[netsockmax].np->mode==RECV)
					{
						ns[netsockmax].r_set = recvset_decode(recvBuffer+sizeof(struct Netprobe));
					}
				if (ns[netsockmax].np->mode==SEND)
					{
						ns[netsockmax].s_set = sendset_decode(recvBuffer+sizeof(struct Netprobe));
					}

				if(ns[netsockmax].np->proto==SOCK_STREAM){
					ns[netsockmax].sockfd = tcpClientSockfd[tcpmax];
					tcpmax++;
					available_tcp++;
				}
				if(ns[netsockmax].np->proto==SOCK_DGRAM){
					//close(tcpClientSockfd[tcpmax]);

					ns[netsockmax].sockfd = udpsockfd;
					udpClientInfo[udpmax] = clientInfo;
					udpmax++;
					available_udp++;

					//we sleep for 1 second because I found it is too fast for client to respond when pktrate is 0
					msleep(1000);
				}
				ns[netsockmax].timer.Start();
				netsockmax++;
			}
			//server_decode()

		}
		sprintf(stat, "TCP Clients [%d], UDP Clients [%d]\n", available_tcp, available_udp);
    }

	return;
}

int main(int argc, char** argv){
	setting(argc, argv);

	//print the stat
	pthread_t t;
	pthread_create(&t, NULL, threadfunc, NULL);

	//set mutex
	if (mode==3)
	{
		selectServer();
	}
	else
	if(mode==4)
	{
		pthread_mutex_init(&queueMutex, 0);

		threadServer();

		pthread_mutex_destroy(&queueMutex);
	}

	return 1;
}