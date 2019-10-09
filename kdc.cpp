#include <netdb.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h> 
#include <iostream>
#include <cmath>
#include "blowfish.h"
#define PORT 9235
#define SA struct sockaddr
// The public keys of initiator A and responder B
#define KEYA 5A7134743777217A25432A462D4A614E
#define KEYB 68576D597133743677397A2443264629
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

int createSocket() {
    int networkSocket, connfd; 
	socklen_t len;
    struct sockaddr_in servaddr, cli; 
  
    // socket create and verification 
    networkSocket = socket(AF_INET, SOCK_STREAM, 0); 
    if (networkSocket == -1) { 
        printf("Socket creation failed...\n"); 
        exit(0); 
    } else {
		printf("Socket successfully created..\n"); 
    }
	bzero(&servaddr, sizeof(servaddr)); 
  
    // assign IP, PORT 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT); 
  
    // Binding newly created socket to given IP and verification 
    if ((bind(networkSocket, (SA*)&servaddr, sizeof(servaddr))) != 0) { 
        printf("Socket bind failed...\n"); 
        exit(0); 
    } else {
        printf("Socket successfully binded..\n"); 
	}
  
    // Now server is ready to listen and verification 
    if ((listen(networkSocket, 5)) != 0) { 
        printf("Listen failed...\n"); 
        exit(0); 
    } else {
        printf("Server listening..\n"); 
    }
	len = sizeof(cli); 
  
    // Accept the data packet from client and verification 
    connfd = accept(networkSocket, (SA*)&cli, &len); 
    if (connfd < 0) { 
        printf("server acccept failed...\n"); 
        exit(0); 
	} else {
		printf("server acccept the client...\n"); 
	}
	
	return connfd;
}	

int main() {
	// Create our network socket
	int networkSocket = createSocket();
	
	Blowfish BF;

	// Close our network socket
	close(networkSocket);
}