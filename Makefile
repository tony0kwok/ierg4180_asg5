SERVER_OUTPUT = server
MAIN_OUTPUT = main
NETPROBE_OUTPUT = netprobe
CLIENT_OUTPUT = client

all: server main client

server: server.cpp
	g++ -std=c++11 server_v2.cpp -lstdc++ -o ${SERVER_OUTPUT} -lpthread

main: main.cpp
	gcc main.cpp -lstdc++ -o ${MAIN_OUTPUT} -lpthread

client: client.cpp
	gcc client.cpp -lstdc++ -o ${CLIENT_OUTPUT} -lpthread

netprobe: netprobe.cpp
	gcc netprobe.cpp -lstdc++ -o ${NETPROBE_OUTPUT} 

clean: 
	rm ${SERVER_OUTPUT} ${MAIN_OUTPUT} ${CLIENT_OUTPUT}