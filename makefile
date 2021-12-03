all: server client

server: ox_server.o ox_lib.o
	gcc -g ox_server.o ox_lib.o -o ox_server

client: ox_client.o
	gcc -g ox_client.o -o ox_client

clean:
	rm -f ox_server ox_client *.o
