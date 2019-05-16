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

#include "netprobe.h"

#define SEND 1
#define RECV 2
#define TCP 1
#define UDP 2

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

int main(int argc, char** argv){
	struct Netprobe np = {SEND, UDP, 1000};
	char *buffer = request_encode(np);
	for (int i = 0; i < sizeof(buffer); i++)
	{
		printf("%02X ", buffer[i]);
	}
	struct Netprobe *new_np = request_decode(buffer);
	printf("%d\n", new_np->proto);
	printf("%ld\n", new_np->bsize);
	return 1;
}