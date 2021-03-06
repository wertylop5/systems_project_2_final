#include "server.h"
#include "../include/protocol.h"
#include "array.h"
#include "log.h"

int max_fd = 0;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

//scans the server socket for a connection, accepts it, and adds that fd to the idle room.
void scan_accept(int listen_sock, struct sockaddr_storage *client_addr , socklen_t *client_addr_size, struct timeval timeout, struct chat_room * idle){
  int new_fd;
  int status;
  char s[INET6_ADDRSTRLEN]; 
  fd_set f;
  fd_set * set = &f;
  FD_ZERO(set);
  FD_SET(listen_sock, set);//add the listening socket to the set
  status = select(max_fd + 1, set, NULL, NULL, &timeout);//check for client connections
  if(status == -1){
    perror("select");
    exit(1);
  }
  if(status > 0){//if there is a connection waiting...
    printf("connection found\n");
    int i;
    new_fd = accept(listen_sock, (struct sockaddr *)client_addr, client_addr_size); //accept new connection and save the socket fd for this connection
    if (new_fd == -1) //check for error
	{
	  perror("accept");
	}
    else{
        is_max(new_fd);
        FD_SET(new_fd, idle->users);//add the new connection to the idle room
        idle->num_users++;
        inet_ntop(client_addr->ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof(s));//convert to human form
        printf("Connection received from %s\n", s);
        struct server_message out;
        pack_msg(&out, MT_COMMAND, "SERVER", "Welcome to the server! Use !help to see commands\n", 0);
        if (send(new_fd, &out, sizeof(struct server_message), 0) == -1) {
            perror("send3");
        }
    }
  }
}

//scans a room for input and handles their message
void scan_room(struct timeval timeout, struct chat_room * room, Array * chatrooms){
    int i = 0;
    int n;//number of bytes received
    struct client_message buff;//buffer for client messages 
    fd_set read_fds = *(room->users);//setting them equal to each other copies users into read_fds
    if((select(max_fd+1, &read_fds, NULL, NULL, &timeout)) == -1){
        perror("select");
        exit(1);
    }
    for(;i<=max_fd; i++){//iterate through all fds that the program has
        if(FD_ISSET(i, &read_fds)){//if i is part of this set...
            if ((n = recv(i, &buff, sizeof(buff), 0)) < 1) {//if error or closed connection...
                if (n == 0) {//closed connection
                    printf("socket %d disconnected\n", i);
                } 
                else{
                   perror("recv");
                }
                close(i); // close the connection because we dont like those who send errors
                FD_CLR(i, room->users); // remove from master set
                room->num_users--;
            }
            else{//we have data
                handle_message(i, buff, room, chatrooms);
            }
        }
    }
}

//handles a client's message according to protocol
void handle_message(int client_fd, struct client_message msg, struct chat_room * room, Array * chatrooms){
    struct server_message out;
    int i;
    unsigned short type = MT_ERR;
    char * username = "SERVER";
    char text[SERVER_MAX_LEN+1];
    memset(text, 0, sizeof(text));
    short in_chatroom = 1;
    print_client_message(msg);
    if((!strcmp(room->name, "IDLE")) && msg.message_type == MT_MESSAGE){
        strcpy(text, "Please join a chat room to send messages\n");
        in_chatroom = 0;
    }
    else if(msg.message_type == MT_COMMAND || msg.message_type == MT_ERR){//if it is a command
        char * command = parse_command(msg.message);
        type=MT_COMMAND;
        printf("Command: %s\n", command);
        if(!strcmp(command, "list")){
            strcpy(text, "List of chatrooms: ");
            for(i=1;i<chatrooms->len;i++){//iterate through chat rooms and exclude IDLE
                strcat(text, (chatrooms->array)[i].name);//concatenate the names of the chatrooms to the message
                strcat(text, " ");
            }
            strcat(text, "\n");
        }
        else if(!strcmp(command, "join")){
            if(!strcmp(msg.chatroom, "IDLE")){
                strcpy(text, "You cannot join the hidden IDLE room. Use !leave\n");
                type = MT_ERR;
            }
            else{
                struct server_message notif;//notification message sent to other users in the chat room
                char notif_msg[SERVER_MAX_LEN];
                int is_new = 0;//tracks if a chat room is new
                struct chat_room * new_room = find_room(msg.chatroom, chatrooms);//search for the requested chat room
                if(new_room == (struct chat_room * )-1){//if the room wasnt found...make a new one
                    new_room = (struct chat_room *)(malloc(sizeof(struct chat_room)));//allocate memory for the chat room
                    strcpy(new_room->name, msg.chatroom);//make the requested name the name of the room
                    new_room->users = malloc(sizeof(fd_set));//allocate memory for new fd_set
                    FD_ZERO(new_room->users);
                    new_room->num_users = 0;
                    insert(chatrooms, new_room);//stick the new room in our list of chat rooms
                    is_new = 1;
                }
                FD_CLR(client_fd, room->users);//remove them from their current room
                room->num_users--;
                FD_SET(client_fd, new_room->users);//add them to the new room's fdset
                if(!is_new){//if it isnt a new room, increment it
                    new_room->num_users++;
                }
                sprintf(text, "You have joined %s. There are currently %d other users in the room.\n", new_room->name, new_room->num_users);
                sprintf(notif_msg, "%s has joined the room\n", msg.username);
                pack_msg(&notif, MT_COMMAND, "SERVER", notif_msg, 1);
                for(i=0; i <=max_fd; i++){//notify other users in the chat room
                    if(FD_ISSET(i, new_room->users) && i != client_fd){//if i is in the room and i isnt the client who called join
                        if (send(i, &notif, sizeof(struct server_message), 0) == -1) {
                            perror("send");
                        }
                    }
                }
            }
        }
        else if(!strcmp(command, "leave")){
            printf("enter leave");
            if(!strcmp(room->name, "IDLE")){
                strcpy(text, "You are already not in a room\n");
            }
            else{
                struct server_message notif;//notification message sent to other users in the chat room
                char notif_msg[SERVER_MAX_LEN];
                FD_CLR(client_fd, room->users);//remove fd from fdset
                room->num_users--;
                printf("leave num_users: %d", room->num_users);
                FD_SET(client_fd, chatrooms->array[0].users);
                chatrooms->array[0].num_users++;
                sprintf(text, "You have left %s\n", room->name);
                sprintf(notif_msg, "%s has left the room\n", msg.username);
                pack_msg(&notif, MT_COMMAND, "SERVER", notif_msg, 1);
                for(i=0; i <=max_fd; i++){//notify other users in the chat room
                    if(FD_ISSET(i, room->users)){//if i is in the room
                        if (send(i, &notif, sizeof(struct server_message), 0) == -1) {
                            perror("send");
                        }
                    }
                }
            }
            in_chatroom = 0;//send message stating that they arent in a chat room anymore
        }
        else if(!strcmp(command, "msg")){//msg sent to other chat rooms
            if(!strcmp(msg.chatroom, "IDLE")){
                type=MT_ERR;
                strcpy(text, "You cannot send messages to the IDLE room\n");
            }
            else{
                type = MT_MESSAGE;
                username = msg.username;
                room = find_room(msg.chatroom, chatrooms);
                command += strlen(command)+1;//get to the rest of the message that has <room> and message
                strsep(&command, " ");//shift to the message
                strcpy(text, command);

                strcpy(msg.message, text);
                write_log(&msg);// write to log

                if(room == (struct chat_room * )-1){//if the room wasnt found
                    type = MT_ERR;
                    strcpy(text,"Chatroom not found\n");
                }
            }
        }
        else if(!strcmp(command, "history")){
            char buffer[SERVER_MAX_LEN + 1];
            char date[10];
            get_date(date, sizeof(date));
            if (read_log(buffer, sizeof(buffer), room->name, date) == -1) {
                strcpy(text, "There is no history for this chatroom.\n");
            } else {
                strcpy(text, "\n");
                strcat(text,buffer);
            }
        }
        else if(!strcmp(command, "help")){
            strcpy(text,"Commands the user can use:\n!list:			list chatrooms\n!join <room>:		join a chatroom or create a new room if <room> is not found\n!leave:			leave the current room\n!msg <room> <message>:	message the indicated room\n!history:		see a log of the messages in the current room\n!help:			lists all available commands\n!disconnect:	disconnects the client from the server\n");
        }
        else{
            strcpy(text,"Command not found\n");
        }
    }
    else{//it is a message to be distributed to other clients
        strcpy(text,msg.message);//copy the user msg into the server message
        username = msg.username;
        type=MT_MESSAGE;
        write_log(&msg);// write to log

    }
    pack_msg(&out, type, username, text, in_chatroom);
    print_server_message(out);
    
    if(out.message_type == MT_COMMAND || out.message_type == MT_ERR){
        if (send(client_fd, &out, sizeof(struct server_message), 0) == -1) {
            perror("send1");
        }
    }
    else{//send to the chat room
        for(i=0; i <=max_fd; i++){
            if(FD_ISSET(i, room->users)){//if i is in the room
                if (send(i, &out, sizeof(struct server_message), 0) == -1) {
                    perror("send2");
                }
            }
        }
    }
}

//takes command, removes ! and new line, and returns 0 if it is just the command, and the msg if it is the !msg command
char * parse_command(char * msg){
    msg++;//move 1 ahead to drop the !
    msg = strip_spaces(msg);//strips spaces and newline at the end
    char * command = strsep(&msg, " ");//split to isolate the command. msg points to whatever is left over
    return command;
}

char * strip_spaces(char * line){
  int len = strlen(line);
  char * end = line+len-1;
  while(*line && isspace(*line)){
    line++;
  }
  while(end > line && isspace(*end)){
    *end-- = '\0';
  }
  return line;
}

int pack_msg(struct server_message * out, unsigned short type, char * username, char * message, short in_chatroom){
    out->message_type = type;
    strcpy(out->username, username);
    strcpy(out->message, message);
    out->in_chatroom = in_chatroom;
    return 1;
}

struct chat_room * find_room(char * target, Array * chatrooms){
    int i;
    for(i=0;i<chatrooms->len;i++){
        if(!strcmp(target, ((chatrooms->array)[i]).name)){
            return &(chatrooms->array)[i];
        }
    }
    return (struct chat_room *)-1;
}

void is_max(int i){
    if(i > max_fd){
        max_fd = i;
    }
}

void print_client_message(struct client_message msg){
    printf("\nCLIENT MESSAGE:\n");
    printf("message type: %d\n", msg.message_type);
    printf("chatroom: %s\n", msg.chatroom);
    printf("username: %s\n", msg.username);
    printf("message: %s\n", msg.message);
    printf("END OF CLIENT MESSAGE\n\n");
}

void print_server_message(struct server_message msg){
    printf("\nSERVER MESSAGE:\n");
    printf("message type: %d\n", msg.message_type);
    printf("username: %s\n", msg.username);
    printf("message: %s\n", msg.message);//doesnt need \n because message already has one
    printf("in_chatroom: %d\n", msg.in_chatroom);
    printf("END OF SERVER MESSAGE\n\n");
}

void print_chat_room(struct chat_room room){
    int i;
    printf("\nCHAT ROOM:\n");
    printf("name: %s\n", room.name);
    printf("users: ");
    for(i=0;i<=max_fd; i++){
        if(FD_ISSET(i, room.users)){
            printf("%d ", i);
        }
    }
    printf("\nnum_users: %d\n", room.num_users);
    printf("END OF CHAT ROOM\n\n");
}

int main()
{
    printf("Starting server setup...\n");

    int listenSocketfd;
    int status;
    struct addrinfo hints;
    struct addrinfo *servinfo; // will point to the results

    memset(&hints, 0, sizeof hints); // make sure the structure is empty to fill with info
    hints.ai_family = AF_UNSPEC;     // We dont care if the address is IPV4 or IPV6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // Automatically assign the address of my local host to the socket structures

    if ((status = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0) //if there is an error with allocating and initializing the linked list of addrinfo structures, then return an error message and exit
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    //servinfo is now a pointer to a linked list of addrinfo structs

    //loop through the linked list and bind to the first working addrinfo struct
    struct addrinfo *p;
    for (p = servinfo; p != NULL; p = p->ai_next)
    {

        //int socket(int domain, int type, int protocol) returns the socket descriptor to be used in later system calls
        if ((listenSocketfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        { //if there is an issue setting up a socket, print the error and exit
            perror("server: socket");
            continue;
        }
        int optval = 1;
        if (setsockopt(listenSocketfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == -1) //allows bind() to reuse local addresses
        {
            perror("setsockopt");
            exit(1);
        }
        // int bind(int sockfd, struct sockaddr *my_addr, int addrlen);
        if ((status = bind(listenSocketfd, servinfo->ai_addr, servinfo->ai_addrlen)) != 0)
        { //if there is an error with binding the socket to a port on the host computer, print the error and exit
            close(listenSocketfd);
            perror("server: bind");
            continue;
        }
        break;
    }

    //Below this point servinfo is of no use so free the linked list from memory -----------------------------------------------------------------------
    freeaddrinfo(servinfo);

    //int listen(int sockfd, int backlog);
    if ((status = listen(listenSocketfd, BACKLOG)) != 0)
    { //if there is an issue listening on a specific port, print the error and exit
        perror("listen");
        exit(1);
    }

    socklen_t lenSockAddr = sizeof(struct sockaddr);
    struct sockaddr_in foo;
    getsockname(listenSocketfd, (struct sockaddr *)&foo, &lenSockAddr);
    fprintf(stderr, "Server is listening on %s:%d\n", inet_ntoa(foo.sin_addr), ntohs(foo.sin_port));

    //main accept loop
    struct sockaddr_storage client_addr; //connector's address information
    socklen_t client_addr_size;          //size of the peer
    int new_fd;                          //new connection put on new_fd
    client_addr_size = sizeof(client_addr);

    struct timeval timeout;//set timeout vals to 0 so it polls instead of waiting in select for connections
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    
    is_max(listenSocketfd);//need to keep track of largest fd for select

    Array chatrooms;//Array is typedef
    init_arr(&chatrooms, MIN_ROOMS);//initialize the chatrooms resizable array
    
    struct chat_room idle;//a room where new connections/fds with no chat room go. just listens for commands.
    strcpy(idle.name,"IDLE");
    idle.num_users = 0;
    fd_set idle_set;
    idle.users = &idle_set;
    insert(&chatrooms, &idle);//idle will be at the 0 index
    
    int i;
    while (1){ //infinite serving loop
      scan_accept(listenSocketfd, &client_addr, &client_addr_size, timeout, &idle);//scan for new connections
      for(i=0;i<chatrooms.len;i++){//iterate through chatrooms
        scan_room(timeout, &chatrooms.array[i], &chatrooms);//handle each individual chatroom
      }
    }
    return 0;
}
