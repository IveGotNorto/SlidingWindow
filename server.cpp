#define _BSD_SOURCE
#include<sys/time.h>
#include <iostream>
#include <string>
#include <unistd.h>         
#include "utilities.h"

int createSocket(string IP, int port) {

    int networkSocket; 
    struct sockaddr_in servaddr; 
  
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

double calculateCustomPing(int packetSize, int receiverSocket){
	float t = 0.125;
	struct timeval start, end;
	char testPacket[packetSize];
	int ack = 0;	
	double rttm, rtts = 0;	

	// Send packet size to receiver
	if(send(receiverSocket, &packetSize, sizeof(int), 0) < 1) {
		perror("Send packet size error.");
		exit(2);
	}
	
	// Perform our first measurement
	gettimeofday(&start, NULL);
	if(send(receiverSocket, testPacket, packetSize, 0) < 1) {
		perror("Send test packet error.");
		exit(2);
	}
	if(reciever(receiverSocket, &ack, sizeof(int)) < 1) {
		perror("Receive ack error.");
		exit(2);
	}
	if(ack == 1) {
		gettimeofday(&end, NULL);
		rttm = (end.tv_usec - start.tv_usec);
		rtts = rttm;
		rtts = (1-t)*rtts + t*rttm;
	} else {
		printf("Bad ack\n");
		exit(2);
	}

	// Perform a few more sends to get an accurate measurement
	for(int i = 0; i != 10; i++) {
		
		gettimeofday(&start, NULL);
		if(send(receiverSocket, testPacket, packetSize, 0) < 1) {
			perror("Send test packet error.");
			exit(2);
		}
		if(reciever(receiverSocket, &ack, sizeof(int)) < 1) {
			perror("Receive ack error.");
			exit(2);
		}
		if(ack == 1) {
			gettimeofday(&end, NULL);
			rttm = (end.tv_usec - start.tv_usec);
			rtts = (1-t)*rtts + t*rttm;
		} else {
			perror("Bad ack");
			exit(2);
		}
		
	}
	
	// Double the measured time
	// TODO Is this necessary / a good idea to double the measured time?
	// Is there a better way to calculate the measured time?
	// Currently using formulas found at: https://www.geeksforgeeks.org/tcp-timers/
	return rtts * 2; 
}

int main() {
	int receiverSocket = createSocket(ip_thing2, PORTREC);	
	int netProtocol, packetSize, timeout, menu, windowSize, sequenceBegin, sequenceEnd, situationalErrors, customPing;	

	// Get settings to be used for file transmission from user
	printf("\nDo you wish to use the default set values or input custom values?\n\n");
	printf("1)Load defaults\n2)Custom Values\n");
	cin >> menu;
	printf("\n");
	if(menu == 1) {
		// Let the receiver know that no ping calculations are needed.
		if(send(receiverSocket, &customPing, sizeof(int), 0) < 1) {
			perror("send ping");
			exit(2);
		} 


		// Set default values here
		netProtocol = 1;
		packetSize = 1500;
		timeout = 15;	
	} else if(menu == 2) {
		// Check which network protocl the user wishes to use
		printf("Which protocol do you wish to use?\n\n");
		printf("1)Stop and Wait\n2)Go-Back-N\n3)Selective Repeat\n");
		cin >> netProtocol;
		printf("\n");	
		if(netProtocol > 3 || netProtocol < 1) {
			printf("Invalid input. Exiting.\n");
			exit(2);
		}

		// Get size of packets users wishes to use
		printf("Size of the packets you wish to use (In bytes): ");
		cin >> packetSize;
		printf("\n");		

		if(packetSize < 1) {
			printf("Invalid packet size. Packet size must be positive. Exiting.\n");
			exit(-1);	
		}else if(packetSize > 1500) {
			printf("Invalid packet size. Packet size is greater than MTU of 1500. Exiting.\n");
			exit(2);
		}

		// Find if user wants to define timeout, and if so set it.
		printf("Do you wish to set a timeout, or use a ping-calculated timeout?\n");
		printf("1)Set a timeout\n2)Ping-calculated timeout\n");
		cin >> timeout;
		printf("\n");

		if(timeout < 1 || timeout > 2) {
			printf("Invalid input. Exiting.\n");
			exit(2);
		}

		if(timeout == 1) {
			if(send(receiverSocket, &customPing, sizeof(int), 0) < 1) {
				perror("send ping");
				exit(2);
			} 

			printf("Input a timeout value (In Microseconds): ");
			cin >> timeout;
		} else {
			customPing = 1;
			if(send(receiverSocket, &customPing, sizeof(int), 0) < 1) {
				perror("send ping");
				exit(2);
			} 
			
			timeout = (int)calculateCustomPing(packetSize, receiverSocket);
			printf("Calculated timeout using given packet size of %d bytes is: %d microseconds\n", packetSize, timeout);
		}

		if(netProtocol != 1) {
			// TODO: Implement menu elements to retrieve remaining params 
			// necessary for GBN and SR.
		}
			
	} else {
		printf("Invalid input. Exiting.");
		exit(2);
	}

	// Open the file we are sending and get its size
	FILE *fp;
	char fileName[MAX];
	fp = getFname("rb", &fileName[0]);
	int fSize = fileSize(fp);
	
	// Build an array of all params to send the receiver.
	int params[8];
	params[0] = fSize;
	params[1] = netProtocol;
	params[2] = packetSize;
	params[3] = timeout;
	params[4] = windowSize;
	params[5] = sequenceBegin;
	params[6] = sequenceEnd;
	params[7] = situationalErrors;	
		

	// Send the parameters to our receiver
	int sent;

	// Find the number of parameters in our array, package, and send that to the receiver.
	int numParams = sizeof(params) / sizeof(params[0]);
	numParams = htonl(numParams);
	if(sent = send(receiverSocket, &numParams, sizeof(int), 0) == -1) {
		perror("send numParams"); 
		exit(2);
	}
	
	// Send the parameters
	if(sent = send(receiverSocket, params, sizeof(params), 0) == -1) {
		perror("send params");
		exit(2);
	}
	
		
	close(receiverSocket);	
	return(1);
}
