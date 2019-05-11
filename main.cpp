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
int bufsize;

int pktrate;

int num;

void encode_header(char *buffer, int number){
	//support maximum sequence number 10000 packages
	sprintf(buffer, "%d%d%d%d", number/1000, (number%1000)/100, (number%100)/10, number%10);
}	

int decode_header(char *buffer){
	return atoi(buffer);
}

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

	stat_ms = 500;
	memset(stat, '\0', sizeof(stat));
	strcpy(stat, "Accepting connection...");

	bsize = 1000;
	bufsize = bsize;

	pktrate = 1000;

	num = 99999;

	const char *optstring = "srp:k:b:u";
    int c;
    struct option opts[] = {
        {"send", 0, NULL, 's'},
        {"recv", 0, NULL, 'r'},
        {"stat", 1, NULL, 'm'},
        {"rport", 1, NULL, 'p'},
        {"pktsize", 1, NULL, 'k'},
        {"rbufsize", 1, NULL, 'b'},
        {"sbufsize", 1, NULL, 'u'},
        {"pktrate", 1, NULL, 't'},
        {"pktnum", 1, NULL , 'n'}
    };
    while((c = getopt_long(argc, argv, optstring, opts, NULL)) != -1) {
        switch(c) {
        	case 'n':
        		num = atoi(optarg);
        		break;
        	case 't':
        		pktrate = atoi(optarg);
        		break;
        	case 'u':
        		bufsize = atoi(optarg);
        		break;
        	case 'b':
        		bufsize = atoi(optarg);
        		break;
        	case 'k':
        		bsize = atoi(optarg);
        		break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'm':
            	stat_ms = atoi(optarg);
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

char *send_stat_cal(int usec_time, int package_no){
	char *output = (char *) malloc(100);
	sprintf(output, "Rate[%.2lfMbps]", (double)(package_no+1)*bsize*1000/usec_time);
	return output;
}

char *recv_stat_cal(int usec_time, int package_no){
	static int recieved = 0, max = 0;
	static int previous_time = 0, pretrans_time = 0;
	static int deltasum = 0;
	double jitter = 0;
	if (package_no>max)
		max = package_no;
	recieved++;
	int lost = package_no+1 - recieved;
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
	sprintf(output, "Pkts [%d] Lost [%d, %.2lf%%] Rate [%.2lfMbps] Jitter [%.2lfus]", recieved, lost, lostrate, rate, jitter);
	return output;
}

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

void tcp_server(){
	char *inputBuffer = (char *) malloc(sizeof(char)*bufsize);
	char *outputBuffer = (char *) malloc(sizeof(char)*bsize);
	memset(inputBuffer, '\0', bufsize);
	memset(outputBuffer, '\0', bsize);
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
    strcpy(stat, "Packet transmitting..");
    
    int recvb = 0;
    int temsum = 0;

    int count = 0;

    ES_Timer timer;
    timer.Start();
    while(1){
    	temsum = 0;
    	memset(outputBuffer, '\0', bsize);
    	while(bsize>temsum){
        	recvb = recv(forClientSockfd,inputBuffer,bufsize,0);
    		if (recvb==0)
    		{
    			printf("Packages all recieved.\n");
    			exit(0);
    		}
    		//printf("BUF SIZE=%d\n", recvb);
    		strcpy(outputBuffer+temsum, inputBuffer);
    		//printf("%s\n", outputBuffer);
    		temsum += recvb;
    	}
    	//printf("This is package %d", decode_header(outputBuffer));
        sprintf(stat, "%s\n", recv_stat_cal(timer.ElapseduSec(),decode_header(outputBuffer)));
    }
}

void tcp_client(){
	//set the package size
	char *message = (char *) malloc(sizeof(char)*bsize);
	memset(message, '\0', sizeof(char)*bsize);


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
    int sendb;
    int temsum = 0;

    ES_Timer timer;
    timer.Start();

    for(int i = 0; i<num; i++){
    	temsum = 0;
    	memset(message,'\n',sizeof(message));
    	encode_header(message, i);

    	//keep sending before put all data of a package into buf
    	while(bsize>temsum){
    		if(pktrate>0)
	    		msleep(1000/(pktrate/bufsize));
	    	sendb = send(sockfd, message+temsum, bufsize>bsize-temsum ? bsize-temsum: bufsize, 0);
	    	temsum += sendb;
	    }
    	sprintf(stat, "%s\n", send_stat_cal(timer.ElapseduSec(), i));
    }
    msleep(stat_ms);
    close(sockfd);
}

void udp_server(){
	char *inputBuffer = (char *) malloc(sizeof(char)*bufsize);
	char *outputBuffer = (char *) malloc(sizeof(char)*bsize);
	memset(inputBuffer, '\0', bufsize);
	memset(outputBuffer, '\0', bsize);
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
    strcpy(stat, "Packet transmitting..");
    
    int recvb = 0;
    int temsum = 0;

    int count = 0;

    ES_Timer timer;
    timer.Start();
    while(1){
    	temsum = 0;
    	memset(outputBuffer, '\0', bsize);
    	while(bsize>temsum){
        	recvb = recv(forClientSockfd,inputBuffer,bufsize,0);
    		if (recvb==0)
    		{
    			printf("Packages all recieved.\n");
    			exit(0);
    		}
    		//printf("BUF SIZE=%d\n", recvb);
    		strcpy(outputBuffer+temsum, inputBuffer);
    		//printf("%s\n", outputBuffer);
    		temsum += recvb;
    	}
    	//printf("This is package %d", decode_header(outputBuffer));
        sprintf(stat, "%s\n", recv_stat_cal(timer.ElapseduSec(),decode_header(outputBuffer)));
    }
}

void udp_client(){
	//set the package size
	char *message = (char *) malloc(sizeof(char)*bsize);
	memset(message, '\0', sizeof(char)*bsize);


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
    int sendb;
    int temsum = 0;

    ES_Timer timer;
    timer.Start();

    for(int i = 0; i<num; i++){
    	temsum = 0;
    	memset(message,'\n',sizeof(message));
    	encode_header(message, i);

    	//keep sending before put all data of a package into buf
    	while(bsize>temsum){
    		if(pktrate>0)
	    		msleep(1000/(pktrate/bufsize));
	    	sendb = send(sockfd, message+temsum, bufsize>bsize-temsum ? bsize-temsum: bufsize, 0);
	    	temsum += sendb;
	    }
    	sprintf(stat, "%s\n", send_stat_cal(timer.ElapseduSec(), i));
    }
    msleep(stat_ms);
    close(sockfd);
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
	
	printf("%s\n", "program ends\n");
}