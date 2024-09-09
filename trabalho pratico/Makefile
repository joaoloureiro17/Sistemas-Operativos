CC = gcc
CFLAGS = -Wall -g

all: void hello orchestrator client

void: void.c
	$(CC) $(CFLAGS) -o void void.c

hello: hello.c
	$(CC) $(CFLAGS) -o hello hello.c

orchestrator: orchestrator.c
	$(CC) $(CFLAGS) -o orchestrator orchestrator.c

client: client.c
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -rf void hello orchestrator client *.dSYM
	rm -f fifo_servidor_* fifoServerClient log.txt
	find . -maxdepth 1 -type d -regex './[0-9]*' -exec rm -rf {} +

