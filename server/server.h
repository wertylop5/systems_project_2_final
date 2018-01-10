#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define MYPORT "3490"   //Port to listen on
#define BACKLOG 1000    //number of connections allowed on the incoming queue
#define RECVBUFFER 3000 //the buffer size for receiving data from client
