CC=gcc
CFLAGS=-Wall
LDFLAGS=-lsqlite3 -L/usr/local/lib
EXEC=main-server host server client

all: main-server host

main-server: main-server.o
	$(CC) -o $@ $^ $(LDFLAGS)

main-server.o: main-server.c main-server.h
	$(CC) $(CFLAGS) -o $@ -c $<

host: host.o client.o server.o
	$(CC) -o $@ $^ $(LDFLAGS)

host.o: host.c client.h server.h
	$(CC) $(CFLAGS) -o $@ -c $<

server.o: server.c  util.h server.h
	$(CC) $(CFLAGS) -o $@ -c $<

client.o: client.c util.h client.h
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -rf *.o

mrproper: clean
	rm -rf $(EXEC)
