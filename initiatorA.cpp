#include <netdb.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> 
#include <iostream>
#include <cmath>
#include "blowfish.h"
#define PORTB 9234
#define PORTKDC 9235
#define SA struct sockaddr 
using namespace std;

char *recieve_all(int sockfd, long *size) {

    long total = 0;
    long bytesleft;
    int r;

    if (r = recv(sockfd, size, sizeof(*size), 0) == -1) {
        perror("recv size");
        exit(1);
    }

    *size = ntohl(*size);
     
    printf("REC: %i, SIZE:%i\n", r, *size);
    char *buf = (char *)malloc(*size * sizeof(char));
    bytesleft = *size;

    r = 0;
    while (total < *size) {
        r = recv(sockfd, buf + total, bytesleft, 0);
        if (r == -1) { break; };
        total += r;
        bytesleft -= r;
    }

    return buf;

}

void send_all(int sockfd, char *buf, long *len) {
    long total = 0;
    long bytesleft = *len;
    int sent;

    long temp = htonl(*len);
    
    if (sent = send(sockfd, &temp, sizeof(long), 0) == -1) {
        perror("send length");
    }
    printf("SENT: %i, SIZE: %li\n", sent, *len);

    sent = 0;
    while (total < *len) {
        sent = send(sockfd, buf + total, bytesleft, 0);
        if (sent == -1) { break; }
        total += sent;
        bytesleft -= sent;
    }
    *len = total;

}

void encrypt(Blowfish bf, long *size, long tsize, char *cont) {

	long dif = *size - tsize;
	// Add padding to end of input
	for (int c = tsize; c < *size; c++) {
		cont[c] = 0x00 + dif;
		/**
		To remove padding, look at the last lines of the file.
		This will give you the number of bytes of padding that
		were added.
		**/
	}

	bf.Encrypt(cont, *size);
}

void decrypt(Blowfish bf, long *size, char *cont) {

	long dif;

	bf.Decrypt(cont, *size);
	// If padding is found, change size
	if ((dif = cont[*size - 1]) < 8) {
		*size -= dif;
	}
}

int createSocket(string IP, int port) {
	int networkSocket, connfd; 
    struct sockaddr_in servaddr, cli; 
  
    // socket create and varification 
    networkSocket = socket(AF_INET, SOCK_STREAM, 0); 
    if (networkSocket == -1) { 
        printf("socket creation failed...\n"); 
        exit(0); 
    } else {
        printf("Socket successfully created..\n"); 
    }
	bzero(&servaddr, sizeof(servaddr)); 
	
    // assign IP, PORT 
    servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = inet_addr(IP.c_str());
	servaddr.sin_port = htons(port); 
  
	int connectionStatus = connect(networkSocket, (SA*)&servaddr, sizeof(servaddr));
  
    // connect the client socket to server socket 
    if (connectionStatus == -1) { 
        printf("connection with the server failed...\n"); 
        exit(0); 
    } else {
        printf("connected to the server..\n"); 
	}
	
	return networkSocket;

}

int main(int argc, char **argv) {
	// Create our network socket
	// bNetworkSocket = thing2
	// kdcNetworkSocket = thing0
	int bNetworkSocket = createSocket("10.35.195.22", PORTB);
	int kdcNetworkSocket = createSocket("10.35.195.46", PORTKDC);
	
	
    // Close sockets
    close(bNetworkSocket);
	close(kdcNetworkSocket);
} 
