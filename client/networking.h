#ifndef NETWORKING_H
#define NETWORKING_H

#include<sys/socket.h>
#include<sys/types.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<string.h>
#include<stdio.h>
#include<unistd.h>

//max length of message
#define MSG_MAX_LEN	256

//prints each address returned from getaddrinfo
void print_addr_list(int write_fd, struct addrinfo *data);

//tests for a valid ip/port pair
int valid_connection(int write_fd, struct addrinfo hint, struct addrinfo **data,
		char *ip, char *port);

#endif
