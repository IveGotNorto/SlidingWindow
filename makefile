CC = g++
CFLAGS  = -std=c++0x -g -Wall

all: responderB kdc initiatorA

responderB: responderB.o blowfish.o utilities.o
	$(CC) $(CFLAGS) -o responderB responderB.o blowfish.o utilities.o

kdc: kdc.o blowfish.o utilities.o
	$(CC) $(CFLAGS) -o kdc kdc.o blowfish.o utilities.o

initiatorA: initiatorA.o blowfish.o utilities.o
	$(CC) $(CFLAGS) -o initiatorA initiatorA.o blowfish.o utilities.o

responderB.o: responderB.cpp
	$(CC) $(CFLAGS) -c responderB.cpp

kdc.o: kdc.cpp
	$(CC) $(CFLAGS) -c kdc.cpp

initiatorA.o: initiatorA.cpp
	$(CC) $(CFLAGS) -c initiatorA.cpp


utilities.o: utilities.cpp utilities.h
	$(CC) $(CFLAGS) -c utilities.cpp

blowfish.o: blowfish.cpp blowfish.h
	$(CC) $(CFLAGS) -c blowfish.cpp

clean:
	$(RM) responderB kdc initiatorA *.o *~ *.swp
