#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
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

int domain;
int type;
int protocol;
int mode;
char address[40];
int port;

int stat_ms;
char stat[200];

int bsize;

int setting(int argc, char** argv){
	//default setting
	mode = 0; //0=server,1=client
	domain = AF_INET;	//AF_INET6
	type = SOCK_STREAM; //SOCK_DGRAM
	protocol = 0;
	memset(address, '\0', sizeof(address));
	char tony[] = "127.0.0.1";
	strcpy(address,tony);
	port = 4180;

	stat_ms = 3000;
	memset(stat, '\0', sizeof(stat));
	strcpy(stat, "Hello World");

	bsize = 1000;

	const char *optstring = "vn:h";
    int c;
    struct option opts[] = {
        {"send", 0, NULL, 's'},
        {"recv", 0, NULL, 'r'},
        {"rport", 1, NULL, 'p'},
        {"pktsize", 1, NULL, 'k'}
    };
    while((c = getopt_long(argc, argv, optstring, opts, NULL)) != -1) {
        switch(c) {
        	case 'k':
        		bsize = atoi(optarg);
        		break;
            case 'p':
                port = atoi(optarg);
                break;
            case 's':
                mode = 1;
                break;
            case 'r':
                mode = 0;
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

void* threadfunc(void* data){
	while(1){
		msleep(stat_ms);
		printf("%s\n", stat);
	}
	return NULL;
}

void tcp_server(){
	char inputBuffer[256] = {};
    int sockfd = 0,forClientSockfd = 0;
    sockfd = socket(AF_INET , SOCK_STREAM , 0);

    if (sockfd == -1){
        printf("Fail to create a socket.");
    }

    //socket的連線
    struct sockaddr_in serverInfo,clientInfo;
    socklen_t addrlen = sizeof(clientInfo);
    bzero(&serverInfo,sizeof(serverInfo));

    serverInfo.sin_family = PF_INET;
    serverInfo.sin_addr.s_addr = INADDR_ANY;
    serverInfo.sin_port = htons(port);
    bind(sockfd,(struct sockaddr *)&serverInfo,sizeof(serverInfo));
    listen(sockfd,5);
    forClientSockfd = accept(sockfd,(struct sockaddr *)&clientInfo, &addrlen);
    
    int remain = 50;
    int recvnum = 0;

    int count = 0;

    while(1){
        recvnum = recv(forClientSockfd,inputBuffer,sizeof(inputBuffer),0);
        count++;
        sprintf(stat, "recieved %d package", count);
        remain -= recvnum;
        if (remain<=0)
        {
        	break;
        }
    }
}

void tcp_client(){
	//set the package size
	char *message = (char *) malloc(sizeof(char)*bsize);
	memset(message, '\n', sizeof(sizeof(char)*bsize));


	int sockfd = socket(domain, type, protocol);

	if (sockfd == -1){
        printf("Fail to create a socket.");
    }

    struct sockaddr_in info;	//set address info
    bzero(&info,sizeof(info));
    info.sin_family = domain;
    info.sin_addr.s_addr = inet_addr(address);
    info.sin_port = htons(port);
    
    int err = connect(sockfd,(struct sockaddr *)&info,sizeof(info));
    if(err==-1){
        printf("Connection error");
    }

    int count = 0;

    for(int i = 0; i<10; i++){
    	msleep(1000);
    	printf("This is %d\n", i);
    	strcpy(message, "hello");
    	send(sockfd, message, 5, 0);
    	count++;
    	sprintf(stat, "sent %d package", count);
    }
}

int main(int argc, char** argv){
	setting(argc, argv);
	pthread_t t;
	pthread_create(&t, NULL, threadfunc, NULL);
	if (mode==0)	//server
	{
		tcp_server();
	}
	else{
		tcp_client();
	}
	

	ES_Timer timer;
	printf("%s\n", "i love world");
}