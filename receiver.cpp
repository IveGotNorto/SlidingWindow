#include <errno.h>
#include <unistd.h> 
#include <iostream>
#include <cmath>
#include <math.h> 
#include <string>         
#include <locale> 
#include "utilities.h"

int createSocket() {

    int networkSocket, connfd; 
    int opt = 1;
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

    if (setsockopt(networkSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
                                                  &opt, sizeof(opt))) 
    { 
        printf("setsockopt failed..."); 
        exit(0);
    }  
    // assign IP, PORT 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORTREC); 
  
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
        printf("server accept failed...\n"); 
        exit(0); 
    } else {
        printf("server accepted the client...\n"); 
    }
    return connfd;
}	

int main() {
	// Create our network socket
	int networkSocket = createSocket();
	
	// Receive the number of params being sent by the server
	int r = 0;
	int numParams = 0;
	if(r = recv(networkSocket, &numParams, sizeof(int), 0) == -1) {
		perror("recv numParams.");
	} 
	
	// Create an array to hold all the parameters sent by the server.
	int params[8];
	
	// Receive the parameters array from the server
	if(r = recv(networkSocket, params, sizeof(int) * 8, 0) == -1) {
		perror("recv params.");
		exit(1);
	}

	printf("The size of the file we'll be transmitting is: %d\n", params[0]);		
	printf("The size of the packet is: %d\n", params[2]);

	close(networkSocket);
}
