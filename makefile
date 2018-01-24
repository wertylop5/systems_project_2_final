client-objs = client.o processes.o networking.o
client-libs = -l ncurses -l cdk

all:
	make client server

run:
	gnome-terminal -e ./server_run
	./client_run

client-debug:
	gcc -g -c client/client.c client/processes.h
	gcc -g -c client/processes.c include/protocol.h client/processes.h client/networking.h
	gcc -g -c client/networking.c client/networking.h include/protocol.h
	gcc -o client_debug $(client-objs) $(client-libs)

client: $(client-objs)
	gcc -o client_run $(client-objs) $(client-libs)

server: server.o
	gcc -o server_run server.o

client.o: client/client.c client/processes.h
	gcc -c client/client.c client/processes.h

processes.o: client/processes.c include/protocol.h client/processes.h client/networking.h
	gcc -c client/processes.c include/protocol.h client/processes.h client/networking.h

networking.o: client/networking.c client/networking.h include/protocol.h
	gcc -c client/networking.c client/networking.h include/protocol.h

server.o: server/server.c include/protocol.h
	gcc -c server/server.c include/protocol.h

clean:
	rm -f *~
	rm -f *.o
	rm -f client_run server_run client_debug test-client test-serv

build-test:
	gcc -o test-client test/client_test.c
	gcc -o test-serv test/serv.c

