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

int main() {
	int receiverSocket = createSocket(ip_thing2, PORTREC);	
	int netProtocol = 0;	
	int packetSize = 0;
	int timeout = 0;
	int menu = 0;	
	int windowSize = 0;
	int sequenceBegin = 0;
	int sequenceEnd = 0;
	int situationalErrors = 0;	

	// Get settings to be used for file transmission from user
	printf("\nDo you wish to use the default set values or input custom values?\n\n");
	printf("1)Load defaults\n2)Custom Values\n");
	cin >> menu;
	printf("\n");
	if(menu == 1) {
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
			exit(-1);
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
			exit(-1);
		}

		// Find if user wants to define timeout, and if so set it.
		printf("Do you wish to set a timeout, or use a ping-calculated timeout?\n");
		printf("1)Set a timeout\n2)Ping-calculated timeout\n");
		cin >> timeout;
		printf("\n");

		if(timeout < 1 || timeout > 2) {
			printf("Invalid input. Exiting.\n");
			exit(-1);
		}

		if(timeout == 1) {
			printf("Input a timeout value (In MS): ");
			cin >> timeout;
		} else {
			// TODO: Probably will need a function created and called here to figure out the 
			// oing-based timeouts.
		}

		if(netProtocol != 1) {
			// TODO: Implement menu elements to retrieve remaining params 
			// necessary for GBN and SR.
		}
			
	} else {
		printf("Invalid input. Exiting.");
		exit(-1);
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
	}
	
	// Send the parameters
	if(sent = send(receiverSocket, params, sizeof(params), 0) == -1) {
		perror("send params");
	}
	
	
	close(receiverSocket);	
}
