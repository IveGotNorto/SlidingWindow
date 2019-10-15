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
#include <math.h> 
#include "blowfish.h"
#define PORT 9251
#define SA struct sockaddr 
#define BSIZE 1024

typedef unsigned char uint8;


#include <arpa/inet.h>
using namespace std;

long f(long nonce) {
    const long A = 48271;
    const long M = 2147483647;
    const long Q = M/A;
    const long R = M%A;

	static long state = 1;
	long t = A * (state % Q) - R * (state / Q);
	
	if (t > 0)
		state = t;
	else
		state = t + M;
	return (long)(((double) state/M)* nonce);
}

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
        printf("socket bind failed...\n"); 
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

int reciever (int sockfd, void *cont, long size) {

    long total = 0;
    long bytesleft = size;
    int rec;

    while (total < size) {
        rec = recv(sockfd, cont + total, bytesleft, 0);
        if (rec == -1) { break; };
        total += rec;
        bytesleft -= rec;
    }

    return total;
}

int main() {
	// Create our network socket
	int networkSocket = createSocket();

    // Up to 32 bit key. This can be adjusted as needed
	char keyB[33];
    char sessionKey[33];
	long nonceB = 0;
	
	Blowfish bf;
	// Simple gui used to parse key and nonce
	while(nonceB < 999999999 || nonceB > 9999999999) {
		printf("\nPlease enter a 10-digit Nonce for B: \n");
		cin >> nonceB;	
	}
	printf("\nPlease enter a Key for B: \n");
	cin >> keyB;
	bf.Set_Passwd(keyB);

    // Our buffer to take the input from A to B containing the session key and ID of A
    char *sendThreeBuffer = (char *)malloc(49);
	long bufferSize = 48;
	sendThreeBuffer = recieve_all(networkSocket, &bufferSize);
    // Decrypt the contents sent from A to B and pull our session key.
    bf.Set_Passwd(keyB);
    decrypt(bf, &bufferSize, sendThreeBuffer);
    string sessionKeyString(&sendThreeBuffer[0], &sendThreeBuffer[32]);
	strcpy(sessionKey, sessionKeyString.c_str());
	bf.Set_Passwd(sessionKey);
    // Package our fourth send of the encrypted nonce of B with the session key
    // Then send this to A for authentication 
    char *sendFourBuffer = (char *)malloc(16);
    bufferSize = 16;
    strcat(sendFourBuffer, to_string(nonceB).c_str());
    encrypt(bf, &bufferSize, bufferSize, sendFourBuffer);
    send_all(networkSocket, sendFourBuffer, &bufferSize);
    free(sendFourBuffer);
    // Receieve fOfB using NonceB from A
    char *sendFiveBuffer = (char *)malloc(16);
    sendFiveBuffer = recieve_all(networkSocket, &bufferSize);
    // Decrypt and verify it
    decrypt(bf, &bufferSize, sendFiveBuffer);
    string fOfBString(&sendFiveBuffer[0], &sendFiveBuffer[6]);
    long fOfBfromA = stol(fOfBString);
	free(sendFiveBuffer);
	
    if(f(nonceB) == fOfBfromA) {
        printf("Authentication between A and B successful. Files may now be sent using the authenticated session key\n");
    }
	
	long *block = (long*)malloc(sizeof(long));
    short *left = (short*)malloc(sizeof(short));
    uint8 *cont = (uint8*)malloc(BSIZE * sizeof(uint8));

    // Recieve needed sizes
    reciever(networkSocket, block, sizeof(long));
    *block = ntohl(*block);

    reciever(networkSocket, left, sizeof(short));
    *left = ntohs(*left);
	
	
	FILE *outputFile = fopen("/tmp/1G-Out2", "a");
        if (!outputFile) {
            perror("File open");
            exit(1);
        }

        printf("Block: %li, Left: %i\n", *block, *left);

        for (long i = 0; i < *block; i++) {

            reciever(networkSocket, cont, BSIZE);
            bf.Decrypt(cont, BSIZE);
            fwrite(cont, BSIZE, 1, outputFile);
            *cont = 0;

        }

        if (*left) {

            reciever(networkSocket, cont, BSIZE);
            bf.Decrypt(cont, BSIZE);
            fwrite(cont, *left, 1, outputFile);

        }

        fclose(outputFile);
        free(cont);
        free(block);
        free(left);
	
	

	// Close our network socket
	close(networkSocket);
}