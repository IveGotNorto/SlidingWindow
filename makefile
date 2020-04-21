CC = g++
CFLAGS = -std=c++0x -g -Wall -pthread

all: NODE_B NODE_A

NODE_B: NODE_B.o utilities.o crc.o
	$(CC) $(CFLAGS) -o NODE_B NODE_B.o utilities.o crc.o

NODE_A: NODE_A.o utilities.o crc.o
	$(CC) $(CFLAGS) -o NODE_A NODE_A.o utilities.o crc.o

NODE_B.o: NODE_B.cpp
	$(CC) $(CFLAGS) -c NODE_B.cpp

NODE_A.o: NODE_A.cpp
	$(CC) $(CFLAGS) -c NODE_A.cpp

utilities.o: utilities.cpp utilities.h
	$(CC) $(CFLAGS) -c utilities.cpp

crc.o: crc.cpp crc.h
	$(CC) $(CFLAGS) -c crc.cpp

clean:
	$(RM) NODE_B NODE_A *.o *~ *.swp
