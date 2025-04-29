CC=g++
CFLAGS=-Wall -Werror -Wno-error=unused-variable -g

build: server subscriber

server: server.cpp
	$(CC) -o $@ $^ $(CFLAGS)

subscriber: subscriber.cpp
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -rf server subscriber
