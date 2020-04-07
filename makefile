CC = g++
CFLAGS  = -std=c++0x -g -Wall

all: server receiver

server: server.o utilities.o
	$(CC) $(CFLAGS) -o server server.o utilities.o

receiver: receiver.o utilities.o
	$(CC) $(CFLAGS) -o receiver receiver.o utilities.o

server.o: server.cpp
	$(CC) $(CFLAGS) -c server.cpp

receiver.o: receiver.cpp
	$(CC) $(CFLAGS) -c receiver.cpp


utilities.o: utilities.cpp utilities.h
	$(CC) $(CFLAGS) -c utilities.cpp

clean:
	$(RM) server receiver *.o *~ *.swp
