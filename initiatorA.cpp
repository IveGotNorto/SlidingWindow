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
#define PORTB 9251
#define PORTKDC 9241
#define SA struct sockaddr 
#define BSIZE 1024   // Block size in bytes
#define BACKLOG 10	 // how many pending connections queue will hold
using namespace std;

typedef unsigned char uint8;

// Get file name and open file
FILE * getFname() {
	char fileName[100];
	scanf("%s", fileName);
	FILE *fp = fopen(fileName, "rb");
	if(!fp) {
		perror("File failed to open");
		exit(-1);
	}	
	return fp;
}

// Function that we pass the nonce to for authentication verification
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
void sender (int sockfd, void *cont, long size) {

    long total = 0;
    long bytesleft = size;
    int sent;
    
    while (total < size) {
        sent = send(sockfd, cont + total, bytesleft, 0);
        if (sent == -1) { 
            perror("send");
            break; 
        }
        total += sent;
        bytesleft -= sent;
    }

    
}

long fileSize (FILE *fp) {

	// Need size of file for dynamic memory allocation
	if (fseek(fp, 0, SEEK_END)) {
		printf("Error: Unable to find end of file\n");
		exit(1);
	}

	long size = ftell(fp);

	printf("File input size: %li (bytes)\n", size);
	// Back to beginning of file;
	fseek(fp, 0L, SEEK_SET);
	return size;

}

int main() {
	// Create our network socket
	// bNetworkSocket = thing2
	int bNetworkSocket = createSocket("10.35.195.22", PORTB);
	// kdcNetworkSocket = thing0
	int kdcNetworkSocket = createSocket("10.35.195.46", PORTKDC);
	string request = "A to B";
	
	// Up to 32 bit key. This can be adjusted as needed
	char keyA[33];
	char sessionKey[33];
	long nonceA = 0;
	
	Blowfish bf;
	// Simple gui used to parse keys, file name, and nonce
	printf("\nPlease enter a file name: \n");
	FILE* fp = getFname();
	while(nonceA < 999999999 || nonceA > 9999999999) {
		printf("\nPlease enter a 10-digit Nonce for A: \n");
		cin >> nonceA;	
	}
	printf("\nPlease enter a Key for A: \n");
	cin >> keyA;
	bf.Set_Passwd(keyA);
	
	// Construct and send our request and Nonce to the KDC
	char *sendOneBuffer = (char *)malloc(16);
	long bufferSize = 16;	
	strcat(sendOneBuffer, request.c_str());
	strcat(sendOneBuffer, to_string(nonceA).c_str());
	send_all(kdcNetworkSocket, sendOneBuffer, &bufferSize);
	free(sendOneBuffer);
	
	// Create our buffer to receieve the second step of authentication
	// The send from the KDC to A containing the encrypted session key,
	// the request, the nonce using Key A as well as the sesssion key,
	// and ID of A using the key of B.
	char *sendTwoBuffer = (char *)malloc(96);
	bufferSize = 96;	
	sendTwoBuffer = recieve_all(kdcNetworkSocket, &bufferSize);
	decrypt(bf, &bufferSize, sendTwoBuffer);
	// Pull our session key and nonce from the buffer sent by the KDC.
	string sessionKeyString(&sendTwoBuffer[0], &sendTwoBuffer[32]);
	strcpy(sessionKey, sessionKeyString.c_str());
	bf.Set_Passwd(sessionKey);
	string tempNonce(&sendTwoBuffer[38], &sendTwoBuffer[48]);
	long nonceFromKDC = stol(tempNonce);

	// Package our send three 
	string sendToB(&sendTwoBuffer[48], &sendTwoBuffer[96]);
	char *sendThreeBuffer = (char *)malloc(sendToB.length() + 1);
	strcpy(sendThreeBuffer, sendToB.c_str());
	free(sendTwoBuffer);
	// Verify the integrity of the communication between A and the KDC
	if(nonceA == nonceFromKDC) {
		printf("\nAuthentication between KDC and A successful\n");
		// Perform our 3rd send this time containing the encrypted session key
		// and ID of A encrypted with B's key to B
		bufferSize = 48;
		send_all(bNetworkSocket, sendThreeBuffer, &bufferSize);
		free(sendThreeBuffer);
		// Receive back the Nonce of B encrypted with the session key
		char *sendFourBuffer = (char *)malloc(16);
		bufferSize = 16;	
		sendFourBuffer = recieve_all(bNetworkSocket, &bufferSize);
		decrypt(bf, &bufferSize, sendFourBuffer);
		// Cut out the nonce from the buffer sent from B, then convert to a long.
		string nonceFromBString(&sendFourBuffer[0], &sendFourBuffer[10]);
		long nonceFromB = stol(nonceFromBString);
		free(sendFourBuffer);
		// Pass our nonce from B through a function, encrypt, and send it back
		long fOfB = f(nonceFromB);
		char *sendFiveBuffer = (char *)malloc(16);
		strcat(sendFiveBuffer, to_string(fOfB).c_str());
		encrypt(bf, &bufferSize, bufferSize, sendFiveBuffer);
		send_all(bNetworkSocket, sendFiveBuffer, &bufferSize);
		free(sendFiveBuffer);
		
		
		
		//Insert Send Large File code
		
		uint8 *cont = (uint8*)malloc(BSIZE * sizeof(uint8));

        long size = fileSize(fp);
        long block = size / BSIZE;
		
		short left = size % BSIZE;

        printf("Size: %li, Block Size: %li, Left: %i\n", size, block, left);
        block = htonl(block);
        sender(bNetworkSocket, &block, sizeof(long));
        left = htons(left);
        sender(bNetworkSocket, &left, sizeof(short));

        for (long i = 0; i < block; i++) {

             fread(cont, BSIZE, 1, fp);
             bf.Encrypt(cont, BSIZE);
             sender(bNetworkSocket, cont, BSIZE);
             *cont = 0;
			}
 
            if (left) {
				fread(cont, left, 1, fp);
                // Pad to 1024
                *cont = *cont << (BSIZE - left);
                bf.Encrypt(cont, BSIZE);
                sender(bNetworkSocket, cont, BSIZE);
				}

                        fclose(fp);
                        free(cont);
		
		//if(testForEncryption)
			
		//elseif(testFor
		
		
		
		
		
		
	} else {
		printf("\nCommunication between KDC and A has been compromised\n");
		exit(-1);
	}

	
    // Close sockets
    //close(bNetworkSocket);
	close(kdcNetworkSocket);
} 
