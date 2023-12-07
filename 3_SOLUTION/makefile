ALL: client server proxy

client: client.c
	gcc -o client client.c

server: server.c
	gcc -o server server.c -lpthread -O0

proxy: proxy.c
	gcc -o proxy proxy.c -lpthread -O0