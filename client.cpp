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
	printf("RecvSet->received: %ld\n", r_set->received);	//wrong name
}

int domain;
int type;
int protocol;
int mode;
char address[40];
int port;
char *port_num;

int stat_ms;
char stat[200];

long bsize;
long bufsize;

long pktrate;

long num;

int proto;

char hostname[100];

int rbufsize;
int sbufsize;


void encode_header(char *buffer, int number){
	//support maximum sequence number 10000 packages
	sprintf(buffer, "%d%d%d%d%d%d%d%d", number/10000000, (number%10000000)/1000000, (number%1000000)/100000, (number%100000)/10000, (number%10000)/1000, (number%1000)/100, (number%100)/10, number%10);
}	

long decode_header(char *buffer){
	return atol(buffer);
}

int setting(int argc, char** argv){
	//default setting
	mode = 0; //0=recv,1=send
	domain = AF_INET;	//AF_INET6
	type = SOCK_DGRAM; //SOCK_DGRAM
	protocol = 0;
	memset(address, '\0', sizeof(address));
	char tony[] = "127.0.0.1";
	strcpy(address,tony);
	port = 4180;
	port_num = (char *)malloc(100);
	strcpy(port_num, "4180");

	stat_ms = 500;
	memset(stat, '\0', sizeof(stat));
	strcpy(stat, "Accepting connection...");

	bsize = 1000;
	bufsize = bsize;
	rbufsize = bsize;
	sbufsize = bsize;
	int bufsize_change = 0;

	pktrate = 1000;

	num = 999999999;

	strcpy(hostname, "localhost");

	const char *optstring = "srem:p:k:b:u:t:n:o:h:l:";
    int c;
    struct option opts[] = {
        {"send", 0, NULL, 's'},
        {"recv", 0, NULL, 'r'},
        {"response", 0, NULL, 'e'},
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
    while((c = getopt_long_only(argc, argv, optstring, opts, NULL)) != -1) {
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
        		num = atol(optarg);
        		if (num==0)
        			num = 999999999;
        		break;
        	case 't':
        		pktrate = atol(optarg);
        		break;
        	case 'u':
        		sbufsize = atol(optarg);
        		bufsize_change = 1;
        		break;
        	case 'b':
        		rbufsize = atol(optarg);
        		bufsize_change = 1;
        		break;
        	case 'k':
        		bsize = atol(optarg);
        		if (bufsize_change == 0)
        			bufsize = bsize;
        		break;
            case 'p':
                port = atoi(optarg);
                strcpy(port_num, optarg);
                break;
            case 'm':
            	stat_ms = atoi(optarg);
            	break; 
            case 's':
                mode = SEND;
                break;
            case 'r':
                mode = RECV;
                break;
            case 'e':
            	mode = 2;
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
	static long received = 0, max = 0;
	static long previous_time = 0, pretrans_time = 0;
	static long deltasum = 0;
	static long lost = 0;
	double jitter = 0;
	if (package_no>max)
		max = package_no;
	received++;
	lost = max+1 - received;
	double lostrate = (double)lost/received;
	double rate = (double)bsize*received*1000/usec_time;
	
	if (received>=2){
		if (int delta = usec_time - previous_time - pretrans_time>0)
			deltasum += delta;
		else
			deltasum -= delta;
	}
	jitter = deltasum/(float)received;
	pretrans_time = usec_time - previous_time;
	previous_time = usec_time;

	char *output = (char *)malloc(100);
	sprintf(output, "Pkts [%ld] Lost [%ld, %.2lf%%] Rate [%.2lfMbps] Jitter [%.2lfus]", received, lost, lostrate, rate, jitter);
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

void client(){
	//set the package size
	char *message = (char *) malloc(sizeof(char)*sbufsize);
	memset(message, '\0', sizeof(char)*sbufsize);

	char *recvBuffer = (char *) malloc(sizeof(char)*rbufsize);
	memset(recvBuffer, '\0', sizeof(char)*rbufsize);

    //if the mode is response
    if (mode==2)
    {
    	struct Netprobe np = {1, type};

	    //set the send set
	    SendSet s_set = {bsize, 0, 1, sbufsize, 0};

	    //set the recv set
		RecvSet r_set = {bsize, rbufsize, 0};

	    struct sockaddr_in info, myaddr;	//set address info
	    socklen_t addrlen = sizeof(info);
	    socklen_t myaddr_addrlen = sizeof(info);
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

	    printf("loading\n");

	    //Request====================
		//send request to tell server TCP or UDP, SEND or RECV
	    
	    int count = 0;
	    int sendb;
	    int temsum = 0;


	    memset(message, '\n', sizeof(message));

    	ES_Timer general_timer;
    	ES_Timer request_timer;
    	long max_us = 0;
    	long min_us = 999999999;
    	long long sum = 0;
    	general_timer.Start();
	    for (int i = 0; i < num; i++)
    	{
    		int sockfd = socket(domain, type, protocol);
			int tcp_sock = socket(domain, SOCK_STREAM, protocol);

			if (sockfd == -1||tcp_sock == -1){
		        printf("Fail to create a socket.");
		        return;
		    }

			int err = connect(tcp_sock,(struct sockaddr *)&info,sizeof(info));
		    if(err==-1){
		        printf("Connection error");
		        exit(1);
		    }

		    getsockname(tcp_sock, (sockaddr *)&myaddr, &myaddr_addrlen);


    		if (pktrate!=0)
    		{
    			if (general_timer.ElapseduSec()>1000000/pktrate)
	    		{
	    			general_timer.Start();
	    		}
	    		else
	    		usleep(100000/pktrate);
    		}
    		

		    char *request_buf;
		    request_buf = request_encode(np);
		    struct Netprobe *new_np = request_decode(request_buf);
		    //if mode is RECV, set host to SEND mode
		    //printf("hostmode = %d, proto = %d\n", new_np->mode?0:1, new_np->proto);
		    //printf("host ip: %s\n", inet_ntoa(info.sin_addr));

		    memset(message,'\n',sizeof(message));
		    memcpy(message, request_buf, sizeof(struct Netprobe));

		    int request_size;

	    	//send send information to server
	    	//memcpy(message+sizeof(struct Netprobe),(const char*) &s_set,sizeof(SendSet));
	    	sendset_encode(message+sizeof(struct Netprobe), s_set);
	    	request_size = sizeof(struct Netprobe)+sizeof(SendSet);

		    /*struct Netprobe *nnp = request_decode(message);
		    showNetprobe(nnp);
		    if (mode==SEND){
			    RecvSet *rr_set = recvset_decode(message+sizeof(struct Netprobe));
			    showRecvSet(rr_set);
			}
		    if (mode==RECV){
			    SendSet *ss_set = sendset_decode(message+sizeof(struct Netprobe));
			    showSendSet(ss_set);
			}*/

		    int sendb_request;
		    request_timer.Start();
			//keep sending before put all data of a package into buf
			if (sendb_request = send(tcp_sock, message, request_size, 0)<=0)
			{
				perror("send request error: ");
			}

			//receive message from server
			//set buffer for storing temporary data
			char *outputBuffer = (char *)malloc(bsize);
			char *inputBuffer = (char *)malloc(rbufsize);


			if (type==SOCK_STREAM)
			{
				//strcpy(stat, "Packet transmitting..");
		    
			    int recvb = 0;
			    int temsum = 0;

			    int count = 0;

			    ES_Timer timer;
			    timer.Start();
			    
			    //recv message
		    	temsum = 0;
		    	memset(outputBuffer, '\0', bsize);
		    	while(bsize>temsum){
		        	recvb = recv(tcp_sock,inputBuffer,bufsize,0);
		        	if (recvb<0)
		        	{
		        		perror("tcp recv error: ");
		        	}
		    		if (recvb==0)
		    		{
		    			printf("Packages all received.\n");
		    			close(tcp_sock);
		    			return;
		    		}
		    		//printf("BUF SIZE=%d\n", recvb);
		    		strcpy(outputBuffer+temsum, inputBuffer);
		    		//printf("%s\n", outputBuffer);
		    		temsum += recvb;
		    	}
		    	r_set.received++;
			    
			}
			else
			if (type==SOCK_DGRAM)
			{
				close(tcp_sock);
				if (bind(sockfd, (struct sockaddr *)&myaddr,
		                            sizeof(myaddr)) <0) {
		            perror("bind failed!");
		            exit(1);
		    	}
		    	ES_Timer timer;
			    timer.Start();
				int temsum;
				int nbytes;
				//strcpy(stat, "Accepting Datagram..");

				temsum = 0;
				memset(outputBuffer, '\0', bsize);
				while(bsize>temsum){

					//sockfd is udp socket, but we use tcp_sock...
					if (nbytes = recvfrom(sockfd, inputBuffer, rbufsize, 0, (struct sockaddr*)&info, &addrlen) < 0) {
		                        perror ("could not read datagram!!");
		                        continue;
		                }
		            strcpy(outputBuffer+temsum, inputBuffer);
		            temsum+=nbytes;

		            //force the loop end
		            temsum=bsize;

	            }
	            //sprintf(stat, "%s\n", recv_stat_cal(timer.ElapseduSec(),decode_header(outputBuffer))); 
				r_set.received++;

			}
			int ress = request_timer.ElapseduSec();
			if (max_us<ress)
			{
				max_us = ress;
			}
			else
			if (min_us > ress)
			{
				min_us = ress;
			}
			sum += ress;
			sprintf(stat, "Replies [%ld] Min [%.1lfms] Max [%.1lfms] Avg [%.1lfms] Jitter [5.2ms]", r_set.received, (double)min_us/1000, (double)max_us/1000, (double)sum/r_set.received/1000);
			//printf("for loop in %d\n", i);
	    }
	}

	//get ipv6 address
	int gaiStatus; // getaddrinfo 狀態碼
    struct addrinfo hints; // hints 參數，設定 getaddrinfo() 的回傳方式
    struct addrinfo *result, *udp_result; // getaddrinfo() 執行結果的 addrinfo 結構指標

    // 以 memset 清空 hints 結構
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET6; // 使用 IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // 串流 Socket
    hints.ai_flags = AI_NUMERICSERV; // 將 getaddrinfo() 第 2 參數 (PORT_NUM) 視為數字

    // 以 getaddrinfo 透過 DNS，取得 addrinfo 鏈結串列 (Linked List)
    // 以從中取得 Host 的 IP 位址
    if ((gaiStatus = getaddrinfo(strcmp(hostname, "localhost")==0?NULL:hostname, port_num, &hints, &result)) != 0)
        errExit((char *) gai_strerror(gaiStatus));

    //get udp address
    hints.ai_socktype = SOCK_STREAM;
    if ((gaiStatus = getaddrinfo(strcmp(hostname, "localhost")==0?NULL:hostname, port_num, &hints, &udp_result)) != 0)
        errExit((char *) gai_strerror(gaiStatus));

    int sockfd = socket(udp_result->ai_family, udp_result->ai_socktype, protocol);
	int tcp_sock = socket(result->ai_family, result->ai_socktype, protocol);

	if (sockfd == -1||tcp_sock == -1){
        printf("Fail to create a socket.");
        return;
    }

    struct sockaddr_in info, myaddr;	//set address info
    socklen_t addrlen = sizeof(info);
    socklen_t myaddr_addrlen = sizeof(info);
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

    printf("loading\n");

    //Request====================
	//send request to tell server TCP or UDP, SEND or RECV
    int err = connect(tcp_sock,result->ai_addr, result->ai_addrlen);
    if(err==-1){
        printf("Connection error");
        exit(1);
    }

    getsockname(tcp_sock, (struct sockaddr *)&myaddr, &myaddr_addrlen);

    int count = 0;
    int sendb;
    int temsum = 0;


    memset(message, '\n', sizeof(message));


    //set netprobe setting request
    struct Netprobe np = {mode?0:1, type};


    //set the send set
    SendSet s_set = {bsize, pktrate, num, sbufsize, 0};

    //set the recv set
    RecvSet r_set = {bsize, rbufsize, 0};


    char *request_buf;
    request_buf = request_encode(np);
    struct Netprobe *new_np = request_decode(request_buf);
    //if mode is RECV, set host to SEND mode
    //printf("hostmode = %d, proto = %d\n", new_np->mode?0:1, new_np->proto);
    printf("host ip: %s\n", inet_ntoa(info.sin_addr));

    memset(message,'\n',sizeof(message));
    memcpy(message, request_buf, sizeof(struct Netprobe));

    int request_size;

    if (mode==SEND)
    {
    	//send recv information to server
    	//memcpy(message+sizeof(struct Netprobe),(const char*) &r_set,sizeof(RecvSet));
    	recvset_encode(message+sizeof(struct Netprobe), r_set);
    	request_size = sizeof(struct Netprobe)+sizeof(RecvSet);
    }
    if (mode==RECV)
    {
    	//send send information to server
    	//memcpy(message+sizeof(struct Netprobe),(const char*) &s_set,sizeof(SendSet));
    	sendset_encode(message+sizeof(struct Netprobe), s_set);
    	request_size = sizeof(struct Netprobe)+sizeof(SendSet);
    }

    /*struct Netprobe *nnp = request_decode(message);
    showNetprobe(nnp);
    if (mode==SEND){
	    RecvSet *rr_set = recvset_decode(message+sizeof(struct Netprobe));
	    showRecvSet(rr_set);
	}
    if (mode==RECV){
	    SendSet *ss_set = sendset_decode(message+sizeof(struct Netprobe));
	    showSendSet(ss_set);
	}*/

    int sendb_request;

	//keep sending before put all data of a package into buf
	if (sendb_request = send(tcp_sock, message, request_size, 0)<=0)
	{
		perror("send request error: ");
	}

	//SendSetsendset_decode(message+sizeof(struct Netprobe));
    //finish request sending===========================


    //check if the server receive the request
    /*while(1){
    	if(int recvb = recv(tcp_sock, recvbuffer, 1000, 0)>0)
    		if(type==SOCK_DGRAM)
    			close(tcp_sock);
    		break;
    }*/

    if (mode==SEND)
    {
	    if(type == SOCK_STREAM){
		   	ES_Timer timer;
		    timer.Start();

		    for(long i = 0; i<num; i++){
		    	temsum = 0;
		    	memset(message,'\n',sizeof(message));
		    	encode_header(message, i);

		    	//keep sending before put all data of a package into buf
		    	while(bsize>temsum){
		    		if(pktrate>0)
			    		msleep(1000/((float)pktrate/sbufsize));
			    	sendb = send(tcp_sock, message+temsum, sbufsize>bsize-temsum ? bsize-temsum: sbufsize, 0);
			    	temsum += sendb;
			    }
			    //printf("socket %ld is sent\n", i);
		    	sprintf(stat, "%s\n", send_stat_cal(timer.ElapseduSec(), i));
		    }
		    msleep(2*stat_ms);
		    close(tcp_sock);	
		}

		if (type==SOCK_DGRAM)
		{
			close(tcp_sock);
			if (bind(sockfd, result->ai_addr, result->ai_addrlen) <0) {
	            perror("bind failed!");
	            exit(1);
	    	}
			int temsum;
			int sendb;
			ES_Timer timer;
		    timer.Start();

		    for(long i = 0; i<num; i++){
		    	temsum = 0;
		    	memset(message,'\n',sizeof(message));
		    	encode_header(message, i);

		    	//keep sending before put all data of a package into buf
		    	while(bsize>temsum){
		    		if(pktrate>0)
			    		msleep(1000/((float)pktrate/sbufsize));
			    	sendb = sendto(sockfd, message+temsum, sbufsize>bsize-temsum ? bsize-temsum: sbufsize, 0, (struct sockaddr *)&info,sizeof(info));
			    	temsum += sendb;
			    }
			    //printf("socket %ld is sent\n", i);
		    	sprintf(stat, "%s\n", send_stat_cal(timer.ElapseduSec(), i));
		    }
		    return;
		}
	}

	if (mode==RECV)
	{
		//set buffer for storing temporary data
		char *outputBuffer = (char *)malloc(bsize);
		char *inputBuffer = (char *)malloc(rbufsize);


		if (type==SOCK_STREAM)
		{
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
		        	recvb = recv(tcp_sock,inputBuffer,bufsize,0);
		        	if (recvb<0)
		        	{
		        		perror("tcp recv error: ");
		        	}
		    		if (recvb==0)
		    		{
		    			printf("Packages all received.\n");
		    			close(tcp_sock);
		    			return;
		    		}
		    		//printf("BUF SIZE=%d\n", recvb);
		    		strcpy(outputBuffer+temsum, inputBuffer);
		    		//printf("%s\n", outputBuffer);
		    		temsum += recvb;
		    	}
		    	//printf("This is package %d", decode_header(outputBuffer));
		        sprintf(stat, "%s\n", recv_stat_cal(timer.ElapseduSec(),decode_header(outputBuffer)));
		        r_set.received++;
		        if (r_set.received>=s_set.num)
		        {
		        	msleep(2*stat_ms);
		        	printf("Packages all received.\n");
		        	close(tcp_sock);
		        	return;
		        }
		    }
		}
		else
		if (type==SOCK_DGRAM)
		{
			close(tcp_sock);
			if (bind(sockfd, result->ai_addr, result->ai_addrlen) <0) {
	            perror("bind failed!");
	            exit(1);
	    	}
	    	ES_Timer timer;
		    timer.Start();
			int temsum;
			int nbytes;
			strcpy(stat, "Accepting Datagram..");
			while(1){
				temsum = 0;
				memset(outputBuffer, '\0', bsize);
				while(bsize>temsum){

					//sockfd is udp socket, but we use tcp_sock...
					if (nbytes = recvfrom(sockfd, inputBuffer, rbufsize, 0, (struct sockaddr*)&info, &addrlen) < 0) {
		                        perror ("could not read datagram!!");
		                        continue;
		                }
		            strcpy(outputBuffer+temsum, inputBuffer);
		            temsum+=nbytes;

		            //force the loop end
		            temsum=bsize;

	            }
	            sprintf(stat, "%s\n", recv_stat_cal(timer.ElapseduSec(),decode_header(outputBuffer))); 
				r_set.received++;
		        if (r_set.received>=s_set.num)
		        {
		        	msleep(2*stat_ms);
		        	printf("Packages all received.\n");
		        	close(sockfd);
		        	return;
		        }
			}
		}
	}
}

int main(int argc, char** argv){
	setting(argc, argv);

	//print the stat
	pthread_t t;
	pthread_create(&t, NULL, threadfunc, NULL);

	//run the main client program
	client();

	printf("%s\n", "program ends\n");
}