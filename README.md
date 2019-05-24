# ierg4180_asg5

## How to run

After cloning the project,
Please open your shell, and input

```
cd {PROJECT_PATH}
```

then make the project
```
make
```

run the server
```
./server -stat 1000 -lhost localhost -lport 4180 -sbufsize 1000 -rbufsize 1000 -ipv6
```

run the client
```
./client --recv --stat 1000 --rport 4180 --proto udp --pktsize 1000 --pktrate 2500 --pktnum 10 --sbufsize 1000 --rbufsize 1000 --rhost google.com -ipv6
```
```
./client --send --stat 1000 --rport 4180 --proto udp --pktsize 1000 --pktrate 2500 --pktnum 10 --sbufsize 1000 --rbufsize 1000 --rhost google.com -ipv6
```

###please notice that the udp mode is not available in localhost environment, thank you
