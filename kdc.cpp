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
#define PORT 9241
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
	string idA = "10.35.195.127";
	char keyA[33] = "5A7134743777217A25432A462D4A614E";
	char keyB[33] = "68576D597133743677397A2443264629";
    string sessionKey = "2939c87950f2420d18c3d3d6b07ab2be";
	Blowfish bf;
	
	// Receiving our 1st send of the request and nonceA
	// Taking in the char array, extracting nonce and request as strings
	// and converting nonce to int
	char *sendOneBuffer = (char *)malloc(17);
	long bufferSize = 17;
	sendOneBuffer = recieve_all(networkSocket, &bufferSize);
    // Pull out the request and nonce of A from the first buffer sent.
	string request(&sendOneBuffer[0], &sendOneBuffer[6]);
	string nonce(&sendOneBuffer[6], &sendOneBuffer[16]);
	free(sendOneBuffer);

	// This is our second send from the KDC back to A.
    // These first few lines of code pack the contents that A will 
    // Be taking in such as the sessionKey, request, and nonce.
	char *sendTwoBuffer = (char *)malloc(96);
	strcat(sendTwoBuffer, sessionKey.c_str());
	strcat(sendTwoBuffer, request.c_str());
	strcat(sendTwoBuffer, nonce.c_str());
    // Then we create the portion we will be sending to B from A
    // This includes the sessionKey and A's ID
	char *eKBKsIDa = (char *)malloc(48);
	strcat(eKBKsIDa, sessionKey.c_str());
	strcat(eKBKsIDa, idA.c_str());
    // It is then encrypted using B's key
	bf.Set_Passwd(keyB);
	bufferSize = 48;
	encrypt(bf, &bufferSize, bufferSize, eKBKsIDa);
    // Concanted onto the contents of what we will be sending to A
    // Then encrypted once more using A's key
	strcat(sendTwoBuffer, eKBKsIDa);
	bf.Set_Passwd(keyA);
    bufferSize = 96;
	encrypt(bf, &bufferSize, bufferSize, sendTwoBuffer);
    // Finally we send that content A to then be decrypted
	send_all(networkSocket, sendTwoBuffer, &bufferSize);

    // Free our buffers
    free(eKBKsIDa);
    free(sendTwoBuffer);

	// Close our network socket
	close(networkSocket);
}