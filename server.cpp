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

long bsize;
long bufsize;

long pktrate;

long long num;

int proto;

char hostname[100];



void encode_header(char *buffer, int number){
	//support maximum sequence number 10000 packages
	sprintf(buffer, "%d%d%d%d%d%d%d%d", number/10000000, (number%10000000)/1000000, (number%1000000)/100000, (number%100000)/10000, (number%10000)/1000, (number%1000)/100, (number%100)/10, number%10);
}	

long long decode_header(char *buffer){
	return atoll(buffer);
}

int setting(int argc, char** argv){
	//default setting
	mode = 0; //0=server,1=client
	domain = AF_INET;	//AF_INET6
	type = SOCK_DGRAM; //SOCK_DGRAM
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
	int bufsize_change = 0;

	pktrate = 1000;

	num = 9999999999999;

	strcpy(hostname, "localhost");

	const char *optstring = "srm:p:k:b:u:t:n:o:h:l:";
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
        {"pktnum", 1, NULL , 'n'},
        {"proto", 1, NULL, 'o'},
        {"rhost", 1, NULL, 'h'},
        {"lhost", 1, NULL, 'l'}
    };
    while((c = getopt_long(argc, argv, optstring, opts, NULL)) != -1) {
        switch(c) {
        	case 'l':
        		strcpy(hostname, optarg);
        		break;
        	case 'h':
        		strcpy(hostname, optarg);
        		break;
        	case 'o':
        		if (strcmp(optarg,"tcp")==0)
        		{
        			type = SOCK_STREAM;
        		}
        		else{
        			type = SOCK_DGRAM;
        		}
        		break;
        	case 'n':
        		num = atoi(optarg);
        		break;
        	case 't':
        		pktrate = atoi(optarg);
        		break;
        	case 'u':
        		bufsize = atoi(optarg);
        		bufsize_change = 1;
        		break;
        	case 'b':
        		bufsize = atoi(optarg);
        		bufsize_change = 1;
        		break;
        	case 'k':
        		bsize = atoi(optarg);
        		if (bufsize_change == 0)
        			bufsize = bsize;
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

char *recv_stat_cal(long usec_time, int package_no){
	static long long recieved = 0, max = 0;
	static long previous_time = 0, pretrans_time = 0;
	static long deltasum = 0;
	static long long lost = 0;
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
    sockfd = socket(AF_INET , type , 0);

    if (sockfd == -1){
        printf("Fail to create a socket.");
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
    bind(sockfd,(struct sockaddr *)&serverInfo,sizeof(serverInfo));

    //tcp
    if(type == SOCK_STREAM){
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

	if (type == SOCK_DGRAM)
	{
		ES_Timer timer;
	    timer.Start();
		int temsum;
		int nbytes;
		strcpy(stat, "Accepting Datagram..");
		while(1){
			temsum = 0;
			memset(outputBuffer, '\0', bsize);
			while(bsize>temsum){
				if (nbytes = recvfrom(sockfd, inputBuffer, bufsize, 0, (struct sockaddr*)&clientInfo, &addrlen) < 0) {
	                        perror ("could not read datagram!!");
	                        continue;
	                }
	            strcpy(outputBuffer+temsum, inputBuffer);
	            temsum+=nbytes;

	            //force the loop end
	            temsum=bsize;

            }
            sprintf(stat, "%s\n", recv_stat_cal(timer.ElapseduSec(),decode_header(outputBuffer))); 
		}
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

    struct sockaddr_in info, myaddr;	//set address info
    bzero(&info,sizeof(info));
    info.sin_family = domain;
    struct hostent *he;
    if ( (he = gethostbyname(hostname) ) == NULL ) {
      exit(1); /* error */
  	}
    memcpy(&info.sin_addr, he->h_addr_list[0], he->h_length);
    //info.sin_addr.s_addr = inet_addr(address);
    info.sin_port = htons(port);

    //myaddr for returning
    bzero((char *)&myaddr, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(0);

    if(type == SOCK_STREAM){
	    int err = connect(sockfd,(struct sockaddr *)&info,sizeof(info));
	    if(err==-1){
	        printf("Connection error");
	        exit(1);
	    }

	    int count = 0;
	    int sendb;
	    int temsum = 0;

	    ES_Timer timer;
	    timer.Start();

	    for(long long i = 0; i<num; i++){
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

	if (type==SOCK_DGRAM)
	{
		if (bind(sockfd, (struct sockaddr *)&myaddr,
                            sizeof(myaddr)) <0) {
            perror("bind failed!");
            exit(1);
    	}
		int temsum;
		int sendb;
		ES_Timer timer;
	    timer.Start();

	    for(long long i = 0; i<num; i++){
	    	temsum = 0;
	    	memset(message,'\n',sizeof(message));
	    	encode_header(message, i);

	    	//keep sending before put all data of a package into buf
	    	while(bsize>temsum){
	    		if(pktrate>0)
		    		msleep(1000/(pktrate/bufsize));
		    	sendb = sendto(sockfd, message+temsum, bufsize>bsize-temsum ? bsize-temsum: bufsize, 0, (struct sockaddr *)&info,sizeof(info));
		    	temsum += sendb;
		    }
	    	sprintf(stat, "%s\n", send_stat_cal(timer.ElapseduSec(), i));
	    }
	}
}

void server(){
	char *sendBuffer = (char *) malloc(sizeof(char)*bufsize);
	char *recvBuffer = (char *) malloc(sizeof(char)*bsize);
	memset(sendBuffer, '\0', bufsize);
	memset(recvBuffer, '\0', bsize);
    int tcpsockfd = 0, udpsockfd = 0;
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

    tcpsockfd = socket(AF_INET , SOCK_STREAM , 0);
    udpsockfd = socket(AF_INET , SOCK_DGRAM , 0);

    if (tcpsockfd == -1 || udpsockfd == -1){
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
    bind(tcpsockfd,(struct sockaddr *)&serverInfo,sizeof(serverInfo));
    bind(udpsockfd,(struct sockaddr *)&serverInfo,sizeof(serverInfo));
    listen(tcpsockfd, 5);

   	//select set up
   	int max = 0;
   	fd_set sock_fds;
   	FD_ZERO(&sock_fds);
    FD_SET(tcpsockfd,&sock_fds);
    if (tcpsockfd>max)
	   max = tcpsockfd;
    FD_SET(udpsockfd,&sock_fds);
    if (udpsockfd>max)
    	max = udpsockfd;

    struct timeval timeout;
    int result;

    while(1){
		if(result = select(max+1, &sock_fds, (fd_set *)0, (fd_set *)0, &timeout)<0){
			perror("select error: ");
			exit(1);
		}
		if (result==0){
			continue;
		}
		if (FD_ISSET(tcpsockfd, &sock_fds))
		{
			printf("Hello\n");
			tcpClientSockfd[tcpmax] =  accept(tcpsockfd,(struct sockaddr *)&clientInfo, &addrlen);
			printf("accepted\n");
			if (recv(tcpClientSockfd[tcpmax], recvBuffer, bsize, 0)>=0)
			{
				printf("recv ed\n");
				printf("%s\n", recvBuffer);
				printf("accepted connection\n");
			}
			//server_decode()

		}
		for (int i = 0; i < tcpmax; ++i)
		{
			/* code */
		}
    }

}

int main(int argc, char** argv){
	setting(argc, argv);
	pthread_t t;
	//pthread_create(&t, NULL, threadfunc, NULL);
	server();
	/*if (mode==0)	//server
	{
		tcp_server();
	}
	else{
		tcp_client();
	}*/
	
	printf("%s\n", "program ends\n");
}