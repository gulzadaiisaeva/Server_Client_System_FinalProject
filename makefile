All:
	gcc server.c -o server -lpthread -w -lm
	gcc clients.c -o clients -lpthread -w -lm
clean:
	rm server
	rm clients
