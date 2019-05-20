CC = g++
CFLAGS = -std=c++11
UNAME := $(shell uname)
SERVER_OUTPUT = server
MAIN_OUTPUT = main
NETPROBE_OUTPUT = netprobe
CLIENT_OUTPUT = client
THREAD_LIB = thread.o

all: server main client

server: server.cpp
	${CC} ${CFLAGS} server_v2.cpp -o ${SERVER_OUTPUT} -lpthread ${THREAD_LIB}

main: main.cpp
	${CC} main.cpp -o ${MAIN_OUTPUT} -lpthread

client: client.cpp
	${CC} client.cpp -o ${CLIENT_OUTPUT} -lpthread 

tinythread: tinythread.cpp
	${CC} -c ${CFLAGS} -o ${THREAD_LIB} tinythread.cpp

netprobe: netprobe.cpp
	${CC} -c ${CFLAGS} -pthread -lrt -o ${NETPROBE_LIB} netprobe.cpp

clean: 
	rm ${SERVER_OUTPUT} ${MAIN_OUTPUT} ${CLIENT_OUTPUT}