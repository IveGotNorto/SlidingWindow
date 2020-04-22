#include "utilities.h"

int sockfd;
unsigned int len;
struct sockaddr_in servaddr; 
int params[7];

typedef struct {
    cli_swp_state ss;
    condition full;
    mutex clear;
    int items;          // # of items for writing.
    int conn;           // Is client connected?
    int finish;         // Is client finished?
} thread_data;

// Only send header frames to server
void clientSend (char *frame) {
    if ( sendto(sockfd, (const char *)frame, HLEN, 
              MSG_WAITALL, (const struct sockaddr *)&servaddr, 
              len) == -1) {
        perror("sendto: ");
    }
}

void *clientListen (void *data) {
    int sawSeqExp = 0;	// Sequence number we are expecting from NODE_A
    int end;
    uint32 chk, tmpChk;
    char buffer[FULL], tmp[HLEN];
    thread_data *td;
    swp_hdr hdr;

    td = (thread_data *) data;
    end = 0;

    while (1) {

        if (recvfrom(sockfd, (char *)buffer, FULL,
                     MSG_WAITALL, ( struct sockaddr *) &servaddr,
                     &len) != -1
            ) {
            // Strip off header
            memcpy(&hdr, &buffer, sizeof(swp_hdr));

            if (hdr.flags == FLAG_HAS_DATA ||
                hdr.flags == FLAG_END_DATA
                ) {

                printf("Packet %i recieved\n", hdr.seqNum);
               
		if(params[0] == 1) {
		    // Stop and Wait
		    // Check if the sequence number sent by NODE_A is what we expected
		    if(hdr.seqNum == sawSeqExp) {		
                    	 // Strip off checksum
                    	memcpy(&tmpChk, &buffer[HLEN + MLEN], sizeof(uint32));
                        tmpChk = ntohl(tmpChk);
                        chk = crcFast((const unsigned char *)&buffer[0], MLEN + HLEN);

			// Check the validity of the checksum
			if(chk == tmpChk) {
			    printf("Checksum OK\n");
			    
			    // Check if this is the last packet
			    if(hdr.flags == FLAG_END_DATA) {
				td->finish = 1;
			    }
			    // Call the writing thread
                            memcpy(td->ss.recvQ[hdr.seqNum].msg, &buffer[HLEN], MLEN);
                            pthread_cond_signal(&td->full);
			    
			    // Send the ACK to NODE_A         	
                    	    tmp[0] = hdr.seqNum;
                    	    tmp[1] = FLAG_ACK_VALID;
                    	    clientSend(&tmp[0]);
                    	    printf("Ack %i sent\n", hdr.seqNum);

			    if(hdr.seqNum) {
				sawSeqExp = 0;
			    } else {
				sawSeqExp = 1;
			    }

			} else {
			    // TODO: Code to respond to bad checksum values
			    printf("Checksum BAD\n");
			}
		    } else {
			// Check what the sequence number of the lost ACK was and resend it
			if(sawSeqExp) {
                    	    tmp[0] = 0;
                    	    tmp[1] = FLAG_ACK_VALID;
                    	    clientSend(&tmp[0]);
                    	    printf("Original Ack lost resending Ack %d\n", 0);
			} else {
                    	    tmp[0] = 1;
                    	    tmp[1] = FLAG_ACK_VALID;
                    	    clientSend(&tmp[0]);
                    	    printf("Original Ack lost resending Ack %d\n", 1);
			}
			pthread_mutex_unlock(&td->clear);
		    }
		} else if(params[0] == 2) {
		    // Go-Back-N
		} else {
		    // Selective Repeat 
                    pthread_mutex_lock(&td->clear);
                    if (swpInWindow(hdr.seqNum, td->ss.FFQ, td->ss.LFQ)) {
                    	// Queue member hasnt been validated yet
                    	if(!(td->ss.recvQ[hdr.seqNum % RWS].valid)) {

                            // Strip off checksum
                            memcpy(&tmpChk, &buffer[HLEN + MLEN], sizeof(uint32));
                            tmpChk = ntohl(tmpChk);
                            chk = crcFast((const unsigned char *)&buffer[0], MLEN + HLEN);

                            // Check checksum values
                            //if (chk == tmpChk) {
                            //    printf("Checksum OK\n");
                            	// Last frame recieved
                            	if (hdr.flags == FLAG_END_DATA) {
                                    // Get end point
                                    end = hdr.seqNum % RWS;
                                    // Signal finish
                                    td->finish = 1;
                            	}

                            	// Store data in recieving queue
                            	memcpy(td->ss.recvQ[hdr.seqNum % RWS].msg, &buffer[HLEN], MLEN);
                            	td->ss.recvQ[hdr.seqNum % RWS].valid = 1;
                            	// Increase item count
                            	td->items++;
                            
                           // } else {
                            
                           //     printf("Checksum failed\n");
                           //     printf("EXP: %X, GOT: %X\n", tmpChk, chk);
                       	   // }
                    	}

                    	tmp[0] = hdr.seqNum;
                    	tmp[1] = FLAG_ACK_VALID;
                    	clientSend(&tmp[0]);
                    	printf("Ack %i sent\n", hdr.seqNum);
                    	// Check if recieve queue is full
                    	// or check for a set end point
                    	if (td->items == RWS ||
                            end && 
                            end == (td->items - 1)
                            ) {
                            printf("Signal write\n");
                            usleep(200);
                            pthread_cond_signal(&td->full);
                    	}

                    }
                    pthread_mutex_unlock(&td->clear);
		}
            } else if (hdr.flags == FLAG_CLIENT_JOIN) {
                // Int's are assumed to be atomic
                td->conn = 1;
            } else if (hdr.flags == FLAG_CLIENT_EXIT) {
                td->conn = 0;
            } else if (hdr.flags == FLAG_SERVER_RTT) {
            	// Strip off checksum
                memcpy(&tmpChk, &buffer[MLEN + HLEN], sizeof(uint32));
                tmpChk = ntohl(tmpChk);
                chk = crcFast((const unsigned char *)&buffer[0], MLEN + HLEN);
                // Check the checksum, if good send the response to the server.
		if (chk == tmpChk) {
		    printf("RTT Packet Checksum good, sending ACK response.\n");
		    tmp[1] = FLAG_CLIENT_RTT;
		    clientSend(&tmp[0]);
		}
            } else if (hdr.flags == FLAG_SERVER_PARAMS) {
		// Receive the parameter values from NODE_A
		char paramBuffer[(sizeof(params) * sizeof(int)) + 8];
		memcpy(&paramBuffer, &buffer[HLEN], sizeof(paramBuffer));
		
		// Use delimited values to cut out and convert strings to int values
		// Then store those values into Params int array			
		size_t pos = 0;
		string paramString(paramBuffer);	
		string delimiter = ".";				
		int i = 0;			
		while((pos = paramString.find(delimiter)) != string::npos) {
		    params[i] = stoi(paramString.substr(0, pos), nullptr, 10);
		    paramString.erase(0, pos + delimiter.length());
		    i++;
		}
	    }

            // Clear buffer
            memset(&buffer, 0, FULL);

        } else {
            perror("recvfrom:");
        }

    }

    return NULL;

}

void *clientWriter (void *data) {

    FILE *fp;
    thread_data *td;
    int saw = 0;

    fp = fopen("/tmp/testFile.txt", "wb");
    td = (thread_data *) data;

    while (!td->finish) {
 

	if(params[0] == 1) { 
	    pthread_mutex_lock(&td->clear);
            // Wait a message to be passed in
            pthread_cond_wait(&td->full, &td->clear);
            
	    if(saw) {
		fwrite(td->ss.recvQ[1].msg, MLEN, 1, fp);
		memset(td->ss.recvQ[1].msg, 0, FULL);
		saw = 0;
	    } else {
		fwrite(td->ss.recvQ[0].msg, MLEN, 1, fp);
		memset(td->ss.recvQ[0].msg, 0, FULL);
		saw = 1;
     	    }
	    pthread_mutex_unlock(&td->clear);
	} else if(params[0] == 2) {

	} else {
            pthread_mutex_lock(&td->clear);
            // Wait for the window to fill
            pthread_cond_wait(&td->full, &td->clear);
            for (int i=0; i < td->items; i++) {
            	fwrite(td->ss.recvQ[i].msg, MLEN, 1, fp); 
            	memset(td->ss.recvQ[i].msg, 0, FULL);
            	td->ss.recvQ[i].valid = 0;
            }
            // Update window size
            td->ss.FFQ = (td->ss.FFQ + RWS) % SN;
            td->ss.LFQ = (td->ss.LFQ + RWS) % SN;
            td->items = 0;
            pthread_mutex_unlock(&td->clear);
	}
    }

    fclose(fp);
    return NULL;

}

// Join & Exit server
void clientComm (thread_data *td, uint8 flag) {

    // Attempts iterator
    uint8 att = 0;
    char tmp[HLEN];

    tmp[0] = 0x0;
    tmp[1] = flag;
    // Make stdout instantly print
    setvbuf(stdout, NULL, _IONBF, 0);
    if (flag == FLAG_CLIENT_JOIN) {
        printf("Connecting to server");
        while (td->conn != 1 &&
               att < ATTEMPTS) {
            clientSend(&tmp[0]);
            printf("...%i", ++att);
            sleep(1);
        }
    } else {
        printf("Exiting server");
        while (td->conn != 0 &&
               att < ATTEMPTS) {
            clientSend(&tmp[0]);
            printf("...%i", ++att);
            sleep(1);
        }
    }
    
    // Reset stdout
    setvbuf(stdout, NULL, _IOLBF, 0);
}

void clientInit (thread_data *td) {

    td->conn = 0;
    td->items = 0;
    td->finish = 0;
    td->ss.FFQ = 0;

    crcInit();
    // Initially set upper bound
    // for window
    td->ss.LFQ = RWS - 1;
    len = sizeof(servaddr);

    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
    
    memset(&servaddr, 0, sizeof(servaddr)); 
    // Filling server information 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons(PORTA); 
    servaddr.sin_addr.s_addr = inet_addr(THING1); 

    // Initialize client window values
    for (int i = 0; i < RWS; i++) {
        td->ss.recvQ[i].msg = (char *)malloc(FULL * sizeof(char));
        td->ss.recvQ[i].valid = 0;
    }
    // Initialize thread stuff
    pthread_mutex_init(&td->clear, NULL);
    pthread_cond_init(&td->full, NULL);

}

void clientDestroy (thread_data *td) {

    for (int i = 0; i < RWS; i++) {
        free(td->ss.recvQ[i].msg);
    }

    pthread_mutex_destroy(&td->clear);
    pthread_cond_destroy(&td->full);

}

int main (void) {
    
    pthread_t threads[NUM_THREADS];
    thread_data td;
    int n;

    clientInit(&td);

    // Create writer
    n = pthread_create(&threads[0], NULL, clientWriter, (void *)&td);
    if (n) {
        printf("Error: Unable to create writer thread.\n");
        exit(-1);
    }

    // Create listener
    n = pthread_create(&threads[1], NULL, clientListen, (void *)&td);
    if (n) {
        printf("Error: Unable to create listener thread.\n");
        exit(-1);
    }

    clientComm(&td, FLAG_CLIENT_JOIN); 

    if (td.conn != 0) {
        printf("...connected.\n"); 
        pthread_join(threads[0], NULL);
        clientComm(&td, FLAG_CLIENT_EXIT);
        if (td.conn == 0) {
            printf("...exited.\n"); 
        }
    }

    if (!td.finish) {
        printf("...failed.\n");
    }

    // Client destroy
    clientDestroy(&td);
    
    return 0;
}
