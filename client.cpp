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

int rbufsize;
int sbufsize;


void encode_header(char *buffer, int number){
	//support maximum sequence number 10000 packages
	sprintf(buffer, "%d%d%d%d%d%d%d%d", number/10000000, (number%10000000)/1000000, (number%1000000)/100000, (number%100000)/10000, (number%10000)/1000, (number%1000)/100, (number%100)/10, number%10);
}	

long long decode_header(char *buffer){
	return atoll(buffer);
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

	stat_ms = 500;
	memset(stat, '\0', sizeof(stat));
	strcpy(stat, "Accepting connection...");

	bsize = 1000;
	bufsize = bsize;
	rbufsize = bsize;
	sbufsize = bsize;
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
        		num = atoi(optarg);
        		break;
        	case 't':
        		pktrate = atoi(optarg);
        		break;
        	case 'u':
        		sbufsize = atoi(optarg);
        		bufsize_change = 1;
        		break;
        	case 'b':
        		rbufsize = atoi(optarg);
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

void client(){
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
    printf("loading\n");
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

	    //send request to tell server TCP or UDP, SEND or RECV
	    memset(message, '\n', sizeof(message));

	    //set netprobe setting request
	    struct Netprobe np = {SEND, SOCK_STREAM};
	    char *h = (char *)malloc(sizeof(hostname));
	    strcpy(h, hostname);
	    SendSet s_set = {h, port, proto, bsize, pktrate, num, sbufsize};
	    RecvSet r_set = {port, proto, bsize, rbufsize, 0};

	    char *request_buf;
	    request_buf = request_encode(np);
	    struct Netprobe *new_np = request_decode(request_buf);
	    printf("mode = %d, proto = %d\n", new_np->mode, new_np->proto);
	    printf("send to host: %s\n", s_set.hostname);

	    for(long long i = 0; i<num; i++){
	    	temsum = 0;
	    	memset(message,'\n',sizeof(message));
	    	encode_header(message, i);

	    	//keep sending before put all data of a package into buf
	    	while(bsize>temsum){
	    		if(pktrate>0)
		    		msleep(1000/(pktrate/sbufsize));
		    	sendb = send(sockfd, message+temsum, sbufsize>bsize-temsum ? bsize-temsum: sbufsize, 0);
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
		    		msleep(1000/(pktrate/sbufsize));
		    	sendb = sendto(sockfd, message+temsum, sbufsize>bsize-temsum ? bsize-temsum: sbufsize, 0, (struct sockaddr *)&info,sizeof(info));
		    	temsum += sendb;
		    }
	    	sprintf(stat, "%s\n", send_stat_cal(timer.ElapseduSec(), i));
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