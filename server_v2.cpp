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

#include "netprobe.h"

#define SEND 1
#define RECV 0

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
	printf("RecvSet->received: %ld\n", r_set->received);	//wrong name
}

void closeNetsock(Netsock *ns){
	ns->sockfd = 0;
	free(ns->s_set);
	free(ns->r_set);
	free(ns->np);
}

int protocol;
int domain;
char address[40];
int port;
int stat_ms;
char stat[200];
long rbufsize;
long sbufsize;
char hostname[100];

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

	stat_ms = 500;
	memset(stat, '\0', sizeof(stat));
	strcpy(stat, "TCP Clients [0], UDP Clients [0]\n");

	rbufsize = 1000;
	sbufsize = 1000;

	strcpy(hostname, "localhost");

	const char *optstring = "m:p:u:f:l:";
    int c;
    struct option opts[] = {
        {"stat", 1, NULL, 'm'},
        {"lport", 1, NULL, 'p'},
        {"sbufsize", 1, NULL, 'u'},
        {"rbufsize", 1, NULL, 'f'},
        {"lhost", 1, NULL, 'l'}
    };
    while((c = getopt_long_only(argc, argv, optstring, opts, NULL)) != -1) {
        switch(c) {
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

/*int tcp_send(int socket, SendSet set){
	int count = 0;
    int sendb;
    int temsum = 0;

    ES_Timer timer;
    timer.Start();

    char *message = (char *)malloc(sbufsize);

    int num = SendSet->num;

    for(long i = 0; i<num; i++){
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
}*/

void server(){
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

	server();

	return 1;
}