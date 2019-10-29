#include <unistd.h> 
#include <iostream>
#include <cmath>
#include <string>         
#include <locale>  
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

    int bNetworkSocket = createSocket(ip_thing2, PORTB);
    int kdcNetworkSocket = createSocket(ip_thing0, PORTKDC);
    string request = "A to B";
    
    // Up to 32 bit key. This can be adjusted as needed
    char keyA[33];
    char sessionKey[33];
    long nonceA = 0;
    string nonceStringA;
    
    Blowfish bf;
    
	bool correctNonceA = false;
    while(nonceStringA.length() !=10 || !correctNonceA) {
        printf("\nPlease enter a 10-digit Nonce for A: ");
        cin >> nonceStringA; 
		
		int numDigits = 0;
		for (std::string::iterator it=nonceStringA.begin(); it!=nonceStringA.end(); ++it)
		{
			
			if (std::isdigit(*it))
			  numDigits++;
			else
			  break;
		  
		}
		
		if(numDigits == 10)
		{
			correctNonceA = true;
		}
    }
    nonceA = stol(nonceStringA);
   
	bool correctKeyA = false;
    while(strlen(keyA) == 0 || !correctKeyA){
        correctKeyA = false;
		printf("\nPlease enter a Key for A: ");
        string tempA;
        cin >> tempA;
    
		int numAlphaCharacters = 0;
		for (std::string::iterator it=tempA.begin(); it!=tempA.end(); ++it)
		{
			if (std::isalnum(*it))
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
    
    bf.Set_Passwd(keyA);
    printf("\nRequest:");
    printf("\nKs for (IDb)");
    printf("\nN1 = %ld\n",nonceA); 

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

    long nonceFromKDC;
    try {
        nonceFromKDC = std::stol(tempNonce, nullptr, 10);
    } catch (const std::exception& e) {
        printf("Key A input on initiator A and Key A input on KDC do not match\n");
        exit(-1);
    }

    printf("\nRecd from KDC");
    printf("\nPrinting N1: %ld", nonceFromKDC);
    printf("\nPrinting Session Key: %s", sessionKeyString.c_str());
    printf("\nSending to IDb\n");

    // Package our send three 
    string sendToB(&sendTwoBuffer[48], &sendTwoBuffer[96]);
    char *sendThreeBuffer = (char *)malloc(sendToB.length() + 1);
    strcpy(sendThreeBuffer, sendToB.c_str());
    free(sendTwoBuffer);

    // Verify the integrity of the communication between A and the KDC
    if(nonceA == nonceFromKDC) {
        
        printf("\nAuthentication with KDC successful.\n");
        close(kdcNetworkSocket);

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

        long nonceFromB;
        try {
            nonceFromB = stol(nonceFromBString);
        } catch (const std::exception& e) {
            printf("Key B input on responder B and Key B input on KDC do not match\n");
            exit(-1);
        }
        free(sendFourBuffer);

        // Pass our nonce from B through a function, encrypt, and send it back
        long fOfB = f(nonceFromB);
        char *sendFiveBuffer = (char *)malloc(16);
        strcat(sendFiveBuffer, to_string(fOfB).c_str());
        encrypt(bf, &bufferSize, bufferSize, sendFiveBuffer);
        send_all(bNetworkSocket, sendFiveBuffer, &bufferSize);
        free(sendFiveBuffer);
        
        printf("\nRecd from IDb");
        printf("\nPrinting N2: %ld",nonceFromB);
        printf("\nPrinting f(N2): %ld",fOfB);
        printf("\nSend to IDb\n");
        
        uint8 *cont = (uint8*)malloc(BSIZE * sizeof(uint8));
        
        FILE *fp;
        char fileName[MAX];
        #if DEBUG
            printf("\nEnter a secret message:\n"); 

            // Get stupid newline out of the buffer
            getchar();
            cin.getline((char*)cont, 1000);
            
            long size = strlen((const char *)cont);
            printf("\nSecret message hex:\n");
            for (int i=0; i < size; i++) {
                printf("%02X", cont[i]);
            } 
            printf("\n");
        #else 
            fp = getFname("rb", &fileName[0]);
            long size = fileSize(fp);
        #endif
        
        long block, tblock;
        block = tblock = size / BSIZE;
        short left, tleft;
        left = tleft = size % BSIZE;
        
        // Network conversions & send
        tblock = htonl(tblock);
        sender(bNetworkSocket, &tblock, sizeof(long));
        tleft = htons(tleft);
        sender(bNetworkSocket, &tleft, sizeof(short));
        
        #if !DEBUG
            for (long i = 0; i < block; i++) {

                 fread(cont, BSIZE, 1, fp);
                 bf.Encrypt(cont, BSIZE);
                 sender(bNetworkSocket, cont, BSIZE);
                 *cont = 0;
            }
        #endif

        *cont = 0;
        if (left) {

            #if !DEBUG
                fread(cont, left, 1, fp);
            #endif

            // Pad to 1024
            *cont = *cont << (BSIZE - left - 1);
            bf.Encrypt(cont, BSIZE);

            #if DEBUG
                printf("\nEncrypted message hex:\n");
                for (int i=0; i < size; i++) {
                    printf("%02X", cont[i]);
                } 
                printf("\n\n");
            #endif

            sender(bNetworkSocket, cont, BSIZE);
        }
        
        #if !DEBUG
            printf("\nFile transmission completed.\n");
            fclose(fp);
        #endif

        close(bNetworkSocket);
        free(cont);
                
    } else {
        close(kdcNetworkSocket);
        close(bNetworkSocket);
        printf("\nCommunication with KDC has been comprimised.\n");
        exit(-1);
    }
    
} 
