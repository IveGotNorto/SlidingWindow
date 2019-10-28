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
    servaddr.sin_port = htons(PORTB); 
  
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

    // Up to 32 bit key. This can be adjusted as needed
    char keyB[33];
    char sessionKey[33];
    long nonceB = 0;
    string nonceStringB;
    Blowfish bf;

	bool correctNonceB = false;
    while(nonceStringB.length() !=10 || !correctNonceB) {
        printf("\nPlease enter a 10-digit Nonce for B: ");
        cin >> nonceStringB;    
		
		int numDigits = 0;
		for (std::string::iterator it=nonceStringB.begin(); it!=nonceStringB.end(); ++it)
		{
			
			if (std::isdigit(*it))
			  numDigits++;
			else
			  break;
		  
		}
		
		if(numDigits == 10)
		{
			correctNonceB = true;
		}
    }
    nonceB = stol(nonceStringB);
    
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

    printf("\nRecd from IDa");
    printf("\nPrinting Session Key: %s",sessionKeyString.c_str());
    printf("\nPrinting N2: %ld",nonceB);
    printf("\nSend to IDa\n");


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

    printf("\nRecd from IDa");
    printf("\nValidating f(N2) on both sides");
    
    long x=f(nonceB);
    
    printf("\nf(N2) on A's side: %ld", fOfBfromA);
    printf("\nf(N2) on B's side: %ld\n", x);
	
    if(x == fOfBfromA) {
        printf("\nAuthentication with A successful.\n");
	
        long *block = (long*)malloc(sizeof(long));
        short *left = (short*)malloc(sizeof(short));
        uint8 *cont = (uint8*)malloc(BSIZE * sizeof(uint8));

        // Recieve needed sizes
        reciever(networkSocket, block, sizeof(long));
        *block = ntohl(*block);
        reciever(networkSocket, left, sizeof(short));
        *left = ntohs(*left);

	FILE *outputFile;
        #if !DEBUG
            char fileName[100];
	    outputFile = getFname("a", &fileName[0]);
	#endif

        // This loop will never be executed during debugging
        for (long i = 0; i < *block; i++) {

            reciever(networkSocket, cont, BSIZE);
            bf.Decrypt(cont, BSIZE);
            fwrite(cont, BSIZE, 1, outputFile);
            *cont = 0;

        }

        if (*left) {

            reciever(networkSocket, cont, BSIZE);

            #if DEBUG
                printf("\nEncrypted message hex:\n");
                for (int i=0; i < *left; i++) {
                    printf("%02X", cont[i]);

                }
                printf("\n");
            #endif    
            bf.Decrypt(cont, BSIZE);
            #if DEBUG
                printf("\nSecret message hex:\n");
                for (int i=0; i < *left; i++) {
                    printf("%02X", cont[i]);

                }
                printf("\n");

                printf("\nSecret message:\n");
                for (int i=0; i < *left; i++) {
                    printf("%c", cont[i]);
                }
                printf("\n");
            #else 
                fwrite(cont, *left, 1, outputFile);
            #endif

        }
        
        #if !DEBUG
            string command = "md5sum ";
            command.append(fileName);

            printf("\nFile transmission completed.\n");
            // run md5sum
            system(command.c_str());
            printf("\n");
            fclose(outputFile);
        #endif
        free(cont);
        free(block);
        free(left);

    } else {
        printf("\nCommunication with A has been comprimised.\n");
        exit(-1);
    }
	
    close(networkSocket);
}
