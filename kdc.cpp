#include <errno.h>
#include <unistd.h> 
#include <iostream>
#include <cmath>
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
    servaddr.sin_port = htons(PORTKDC); 

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
        printf("server accept failed...\n"); 
        exit(0); 
    } else {
        printf("server accept the client...\n"); 
    }
    
    return connfd;
}	

int main() {

    Blowfish bf;
    char keyA[33];
    char keyB[33];
    string sessionKey;

    int networkSocket = createSocket();

    bool correctKeyA = false;
	while(strlen(keyA) == 0 || !correctKeyA){
		correctKeyA = false;       
	   printf("\nPlease enter a Key for A: ");
        string tempA;
        cin >> tempA;

		int numAlphaCharacters = 0;
		for (std::string::iterator it=tempA.begin(); it!=tempA.end(); ++it)
		{
			if (std::isalpha(*it))
			  numAlphaCharacters++;
			else
			  break;
		  
		}
		
		if(numAlphaCharacters == tempA.length())
		{
			correctKeyA = true;
		}
		
        if(tempA.length()>32)
        {
            printf("\nKey must be 32 characters or less");
        }
        else if(tempA.length()<=32)
        {
            for(int z=0;z<tempA.length();z++)
            {
                keyA[z]=tempA.at(z);
            }
            
            int count=0;
            for(int x=tempA.length();x<32;x++)
            {
                keyA[x]=tempA.at(count);
                count++;
                if(count==tempA.length())
                {
                    count=0;
                }
            }
        }
    
    }
    
	bool correctKeyB = false;
    while(strlen(keyB) == 0 || !correctKeyB){
		correctKeyB = false;
        printf("\nPlease enter a Key for B: ");
        string tempB;
        cin >> tempB;
    
		int numAlphaCharacters = 0;
		for (std::string::iterator it=tempB.begin(); it!=tempB.end(); ++it)
		{
			if (std::isalpha(*it))
			  numAlphaCharacters++;
			else
			  break;
		  
		}
		
		if(numAlphaCharacters == tempB.length())
		{
			correctKeyB = true;
		}
		
        if(tempB.length()>32)
        {
            printf("\nKey must be 32 characters or less");
        }
        else if(tempB.length()<=32)
        {
            for(int z=0;z<tempB.length();z++)
            {
                keyB[z]=tempB.at(z);
            }
            
            int count=0;
            for(int x=tempB.length();x<32;x++)
            {
                keyB[x]=tempB.at(count);
                count++;
                if(count==tempB.length())
                {
                    count=0;
                }
            }
        }
    }
    bool correctKeySession = false;
    while(sessionKey.length() == 0 || !correctKeySession){
        correctKeySession = false;
		sessionKey.reserve(32);
        printf("\nPlease enter a Session Key: ");
        cin >> sessionKey;
    
		int numAlphaCharacters = 0;
		for (std::string::iterator it=sessionKey.begin(); it!=sessionKey.end(); ++it)
		{
			if (std::isalpha(*it))
			  numAlphaCharacters++;
			else
			  break;
		  
		}
		
		if(numAlphaCharacters == sessionKey.length())
		{
			correctKeySession = true;
		}
		
        if(sessionKey.length()>32)
        {
            printf("\nKey must be 32 characters or less");
        }
        else if(sessionKey.length()<=32)
        {
           
            int count=0;
            for(int x=sessionKey.length();x<32;x++)
            {
                
                sessionKey[x]=sessionKey.at(count);
                count++;
                if(count==sessionKey.length())
                {
                    count=0;
                }
            }
        }
    } 

    // Receiving our 1st send of the request and nonceA
    char *sendOneBuffer = (char *)malloc(17);
    long bufferSize = 17;
    sendOneBuffer = recieve_all(networkSocket, &bufferSize);

    // Pull out the request and nonce of A from the first buffer sent.
    string request(&sendOneBuffer[0], &sendOneBuffer[6]);
    string nonce(&sendOneBuffer[6], &sendOneBuffer[16]);
    free(sendOneBuffer);

    printf("\nRecd from IDa");
    printf("\nRequesting Ks for IDb");
    printf("\nN1 recd = %s\n",nonce.c_str());

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
    strcat(eKBKsIDa, ip_thing1.c_str());

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

    printf("\nSend to IDa");
    printf("\nPrinting N1: %s", nonce.c_str());
    printf("\nPrinting Session Key: %s\n", sessionKey.c_str());


    // Free our buffers
    free(eKBKsIDa);
    free(sendTwoBuffer);

    // Close our network socket
    close(networkSocket);
}
