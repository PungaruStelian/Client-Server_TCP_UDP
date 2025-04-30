CC=g++
CFLAGS=-Wall -Werror -Wno-error=unused-variable -g

build: server subscriber

# Server executable
server: server.cpp common.cpp server.h common.h
	$(CC) -o $@ server.cpp common.cpp $(CFLAGS)

# Subscriber executable
subscriber: subscriber.cpp common.cpp subscriber.h common.h
	$(CC) -o $@ subscriber.cpp common.cpp $(CFLAGS)

# Clean temporary files and binaries
clean:
	rm -f server subscriber *.o *.gch