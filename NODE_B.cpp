#include "utilities.h"

int sockfd;
unsigned int len;
struct sockaddr_in servaddr; 
int params[4];

typedef struct {
    cli_swp_state ss;
    semaphore full;
    mutex slide;
    int conn;           // Is client connected?
    int numPackets;
    int droppedPackets;
    int lastSequenceNum;
} thread_data;

// Only send header frames to server
void clientSend (char *frame) {
    if ( sendto(sockfd, (const char *)frame, HLEN, 
              MSG_WAITALL, (const struct sockaddr *)&servaddr, 
              len) == -1) {
        perror("sendto: ");
    }
}

void *clientListener (void *data) {
    int iFrame;
    int seqExp = 0;	// Sequence number we are expecting from NODE_A
    uint32 chk, tmpChk;
    char buffer[FULL], tmp[HLEN];
    thread_data *td;
    swp_hdr hdr;
    td = (thread_data *) data;
    td->droppedPackets = 0;
    td->numPackets = 0;

    while (1) {

        chk = 0;
        tmpChk = 0;

        if (recvfrom(sockfd, (char *)buffer, FULL,
                     MSG_WAITALL, ( struct sockaddr *) &servaddr,
                     &len) != -1
            ) {

            // Strip off header
            memcpy(&hdr, &buffer[0], sizeof(swp_hdr));
            printf("Packet %i recieved\n", hdr.seqNum);
                               
	       //  printf("Checksum OK\n");

                if (hdr.flags == FLAG_HAS_DATA ||
                    hdr.flags == FLAG_END_DATA
                    ) {
                    if(params[0] == 1) {
                        // Stop and Wait
                        // Check if the sequence number sent by NODE_A is what we expected
                        if(hdr.seqNum == seqExp) {

            		    // Strip off checksum
                            memcpy(&tmpChk, &buffer[HLEN + MLEN], sizeof(uint32));

            		    tmpChk = ntohl(tmpChk);
            		    chk = crcSlow((const unsigned char *)&buffer[0], MLEN + HLEN);
			    // Validate checksum    
			    if(tmpChk == chk) {
				printf("Checksum OK\n");
                            	// Copy frame data to queue
                            	memcpy(td->ss.recvQ[0].msg, &buffer[sizeof(swp_hdr)], params[1]);
			    	// Pull the frames header info off
                            	td->ss.recvQ[0].size = ntohs(hdr.size);
                            	td->ss.recvQ[0].ackNum = hdr.seqNum;
                            	td->ss.recvQ[0].flags = hdr.flags;
                            	td->ss.recvQ[0].valid = 1;

                            	// Set the ACK data         	
                            	tmp[0] = hdr.seqNum;
                            	tmp[1] = FLAG_ACK_VALID;
			  
			    	// Lock the thread, and wait for the writer to write the data to the file. 
			    	pthread_mutex_lock(&td->slide);
		            	while(td->ss.recvQ[0].valid);
			    	pthread_mutex_unlock(&td->slide);
			    	// Send our Ack to the server
			    	clientSend(&tmp[0]);
                           	printf("Ack %i sent.\n", hdr.seqNum);
			    	td->numPackets = td->numPackets + 1; 
				// Update the sequence number we are expecting from the client
                            	if(hdr.seqNum) {
                                    seqExp = 0;
                            	} else {
                                    seqExp = 1;
                            	}	
			    } else {
				// Failed checksum
				printf("Checksum failed\n");
			    }
                        } else {
			    pthread_mutex_lock(&td->slide);
                            // Check what the sequence number of the lost ACK was and resend it
                            if(seqExp) {
                                tmp[0] = 0;
                                tmp[1] = FLAG_ACK_VALID;
                                clientSend(&tmp[0]);
				td->droppedPackets = td->droppedPackets + 1;
                                printf("Original Ack lost or Packet lost resending Ack %d\n", 0);
                            } else {
                                tmp[0] = 1;
                                tmp[1] = FLAG_ACK_VALID;
                                clientSend(&tmp[0]);
				td->droppedPackets = td->droppedPackets + 1;
                                printf("Original Ack lost or Packet lost resending Ack %d\n", 1);
                            }
			    pthread_mutex_unlock(&td->slide);
                        }
                    } else if(params[0] == 2) {
                        // Go-Back-N
                        // Check if the sequence number sent by NODE_A is what we expected
                        if(hdr.seqNum == seqExp) {
            		   // Strip off checksum
                           memcpy(&tmpChk, &buffer[HLEN + MLEN], sizeof(uint32));

            		   tmpChk = ntohl(tmpChk);
            		   chk = crcSlow((const unsigned char *)&buffer[0], MLEN + HLEN);
			    // Validate checksum   
			    if(tmpChk == chk) {
				printf("Checksum OK\n");
                            	// Copy frame data to queue
                            	memcpy(td->ss.recvQ[0].msg, &buffer[sizeof(swp_hdr)], params[1]);
			    	// Pull the frames header info off
                            	td->ss.recvQ[0].size = ntohs(hdr.size);
                            	td->ss.recvQ[0].ackNum = hdr.seqNum;
                            	td->ss.recvQ[0].flags = hdr.flags;
                            	td->ss.recvQ[0].valid = 1;

                            	// Set the ACK data         	
                            	tmp[0] = hdr.seqNum;
                            	tmp[1] = FLAG_ACK_VALID;
			  
			    	// Lock the thread, and wait for the writer to write the data to the file. 
			    	pthread_mutex_lock(&td->slide);
		            	while(td->ss.recvQ[0].valid);
			    	pthread_mutex_unlock(&td->slide);
			    	// Send our Ack to the server
			    	clientSend(&tmp[0]);
                           	printf("Ack %i sent.\n", hdr.seqNum);
			    	td->numPackets = td->numPackets + 1; 
				// Check if we have reached the end of the transmission
				if(hdr.flags == FLAG_END_DATA) {
				    td->lastSequenceNum = hdr.seqNum;
				}
				// Increment expect packet, and print our current window size
				seqExp = (++seqExp) % SN;
				printf("Current window = [%d]\n", seqExp);
			    } else {
				// Checksum failed, print message and current window
				printf("Checksum failed\n");
				printf("Current window = [%d]\n", seqExp);
			    }
                        } else {
			    if(hdr.seqNum < seqExp) {
			    	pthread_mutex_lock(&td->slide);
                            	// Resend the hight valid ACK that we have if a packet with less
                            	// than our highest ACK valued comes in.
                           	tmp[0] = seqExp - 1;
                            	tmp[1] = FLAG_ACK_VALID;
                            	clientSend(&tmp[0]);
		            	td->droppedPackets = td->droppedPackets + 1;
                            	printf("Packet with less than expected sequence received. Resending highest previous Ack %d\n", seqExp - 1);
			    	pthread_mutex_unlock(&td->slide);
			    } else if(hdr.seqNum > seqExp) {
				// If a packet comes in with a higher than valid sequence, just ignore
				printf("Packet with higher than expected sequence received.\n");
				td->droppedPackets = td->droppedPackets + 1;
			    }
			}
                    } else {
                        pthread_mutex_lock(&td->slide);
                        if (swpInWindow(hdr.seqNum, td->ss.FFQ, td->ss.LFQ)) {
            
                            // Find frame index
                            iFrame = 0;       
                            while (hdr.seqNum != td->ss.recvQ[iFrame].ackNum) {
                               iFrame = (++iFrame) % RWS;
                            }

                            // Queue member hasnt been validated yet
                            if(!(td->ss.recvQ[iFrame].valid)) {
                                
                                // Wait for space to put frame
                                sem_wait(&td->full);  
                                // Copy frame data to queue
                                memcpy(td->ss.recvQ[iFrame].msg, &buffer[sizeof(swp_hdr)], MLEN);

				if(hdr.flags == FLAG_END_DATA) {
				    td->lastSequenceNum = hdr.seqNum;
				}

                                td->ss.recvQ[iFrame].size = ntohs(hdr.size);
                                td->ss.recvQ[iFrame].ackNum = hdr.seqNum;
                                td->ss.recvQ[iFrame].flags = hdr.flags;
                                td->ss.recvQ[iFrame].valid = 1;
				td->numPackets = td->numPackets + 1;
                                tmp[0] = hdr.seqNum;
                                tmp[1] = FLAG_ACK_VALID;
                                clientSend(&tmp[0]);
                                printf("Ack %i sent\n", hdr.seqNum);
                
                            }
                        }
                        pthread_mutex_unlock(&td->slide);
                    }

                } else if (hdr.flags == FLAG_SERVER_RTT) {
                    // Check the checksum, if good send the response to the server.
                    printf("RTT Packet received, sending ACK response.\n");
                    tmp[1] = FLAG_CLIENT_RTT;
                    clientSend(&tmp[0]);
                } else if (hdr.flags == FLAG_SERVER_PARAMS) {
                    // Receive the parameter values from NODE_A
                    char paramBuffer[(sizeof(params) * sizeof(int)) + 8];
                    memcpy(&paramBuffer, &buffer[2], sizeof(paramBuffer));
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
                } else if (hdr.flags == FLAG_CLIENT_JOIN) {
                    // Int's are assumed to be atomic
                    td->conn = 1;
                } else if (hdr.flags == FLAG_CLIENT_EXIT) {
                    td->conn = 0;
                }


            // Clear buffer
            memset(&buffer[0], 0, FULL);

        } else {
            perror("recvfrom:");
        }

    }

    return NULL;

}

void *clientWriter (void *data) {

    int end;
    FILE *fp;
    thread_data *td;
    int i = 0;
    fp = fopen("/tmp/AmazingAwesomeTestFile", "wb");
    td = (thread_data *) data;
    end = 0;

    while (!end) {

	if(params[0] != 3) { 
            // Wait for a message to reach the queue
            if (td->ss.recvQ[0].valid) {
		// Write the message, reset the buffer
                fwrite(td->ss.recvQ[0].msg, td->ss.recvQ[0].size, 1, fp);
                memset(td->ss.recvQ[0].msg, 0, FULL);
                td->ss.recvQ[0].valid = 0;
		td->ss.recvQ[0].size = 0;
		// Check if we have finished the transmission of data
		if(td->ss.recvQ[0].flags == FLAG_END_DATA) {
		    end = 1;
		}
		// Reset the flag, signaling listener to send ACK to server
                td->ss.recvQ[0].flags = 0;
            }
	} else {
            // Wait for valid slot to open
            if (td->ss.recvQ[0].valid) {
                pthread_mutex_lock(&td->slide);
                if (fwrite(td->ss.recvQ[0].msg, td->ss.recvQ[0].size, 1, fp) != 1) {
                    perror("write: ");
                }
                
                if (td->ss.recvQ[0].flags == FLAG_END_DATA) {
                    end = 1;
                } else {

                    for (int c = 0; c < RWS-1; c++) {
                        memcpy(td->ss.recvQ[c].msg, td->ss.recvQ[c+1].msg, MLEN);
                        td->ss.recvQ[c].valid = td->ss.recvQ[c+1].valid;
                        td->ss.recvQ[c].ackNum = td->ss.recvQ[c+1].ackNum;
                        td->ss.recvQ[c].size = td->ss.recvQ[c+1].size;
                        td->ss.recvQ[c].flags = td->ss.recvQ[c+1].flags;
                    }

                    memset(td->ss.recvQ[RWS-1].msg, 0, MLEN);
                    td->ss.recvQ[RWS-1].valid = 0;
                    td->ss.recvQ[RWS+1].flags = 0;
                    td->ss.recvQ[RWS+1].size = 0;

                    // Update window size
                    td->ss.FFQ = (++td->ss.FFQ) % SN;
                    td->ss.LFQ = (++td->ss.LFQ) % SN;

                    td->ss.recvQ[RWS-1].ackNum = td->ss.LFQ;
                }
                pthread_mutex_unlock(&td->slide);
                sem_post(&td->full);
            }

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
    td->ss.FFQ = 0;

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
    servaddr.sin_addr.s_addr = inet_addr(THING0); 

    // Initialize client window values
    for (int i = 0; i < RWS; i++) {
        td->ss.recvQ[i].msg = (char *)malloc(MLEN);
        td->ss.recvQ[i].ackNum = i;
        td->ss.recvQ[i].valid = 0;
        td->ss.recvQ[i].flags = 0;
        td->ss.recvQ[i].size = 0;
    }
    // Initialize thread stuff
    pthread_mutex_init(&td->slide, NULL);
    sem_init(&td->full, 0, RWS); 
}

void clientDestroy (thread_data *td) {
    // Print our end of transmission logs
    if(params[0] != 1) {
	printf("Last packet seq# received: %d\n", td->lastSequenceNum);
    }
    printf("Number of original packets received: %d\n", td->numPackets);
    printf("Number of retransmitted packets received: %d\n", td->droppedPackets);

    if(params[0] == 3) {
        for (int i = 0; i < RWS; i++) {
    	    free(td->ss.recvQ[i].msg);
	}
    }
    pthread_mutex_destroy(&td->slide);
    sem_destroy(&td->full);

}

int main (void) {
    
    pthread_t threads[NUM_THREADS];
    thread_data td;
    int n;

    clientInit(&td);

    // Create listener
    n = pthread_create(&threads[1], NULL, clientListener, (void *)&td);
    if (n) {
        printf("Error: Unable to create listener thread.\n");
        exit(-1);
    }

    // Communicate to server
    clientComm(&td, FLAG_CLIENT_JOIN); 

    if (td.conn == 1) {

        // Create writer
        n = pthread_create(&threads[0], NULL, clientWriter, (void *)&td);
        if (n) {
            printf("Error: Unable to create writer thread.\n");
            exit(-1);
        }

        printf("...connected.\n"); 
        pthread_join(threads[0], NULL);
        clientComm(&td, FLAG_CLIENT_EXIT);
        if (td.conn == 0) {
            printf("...exited.\n"); 
        } else {
            printf("...failed.\n");
        }
    } else {
        printf("...failed.\n");
    }

    // Client destroy
    clientDestroy(&td);
    
    return 0;
}
