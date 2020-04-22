#include "utilities.h"

int sockfd;
unsigned int len;
struct sockaddr_in servaddr; 
int params[7];

typedef struct {
    cli_swp_state ss;
    semaphore full;
    mutex slide;
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

    int iFrame;
    uint32 chk, tmpChk;
    char buffer[FULL], tmp[HLEN];
    thread_data *td;
    swp_hdr hdr;

    td = (thread_data *) data;

    while (1) {

        if (recvfrom(sockfd, (char *)buffer, FULL,
                     MSG_WAITALL, ( struct sockaddr *) &servaddr,
                     &len) != -1
            ) {
            // Strip off header
            memcpy(&hdr, &buffer[0], sizeof(swp_hdr));

            if (hdr.flags == FLAG_HAS_DATA ||
                hdr.flags == FLAG_END_DATA
                ) {

                printf("Packet %i recieved\n", hdr.seqNum);

                pthread_mutex_lock(&td->slide);
                if (swpInWindow(hdr.seqNum, td->ss.FFQ, td->ss.LFQ)) {
    
                    // Find frame index
                    iFrame = 0;       
                    while (hdr.seqNum != td->ss.recvQ[iFrame].ackNum) {
                       iFrame = (++iFrame) % RWS;
                    }

                    // Queue member hasnt been validated yet
                    if(!(td->ss.recvQ[iFrame].valid)) {
                                      
                        tmpChk = 0;
                        chk = 0;
                        // Strip off checksum
                        memcpy(&tmpChk, &buffer[HLEN + MLEN], sizeof(uint32));
                        tmpChk = ntohl(tmpChk);
                        chk = crcSlow((const unsigned char *)&buffer[0], MLEN + HLEN);

                        // Check checksum values
                        if (chk == tmpChk) {
                            // Wait for space to put frame
                            sem_wait(&td->full);  

                            // Clear out frame -- not doing this was creating issues...
                            memset(td->ss.recvQ[iFrame].msg, 0, MLEN);
                            // Copy frame data to queue
                            memcpy(td->ss.recvQ[iFrame].msg, &buffer[HLEN], MLEN);
                            td->ss.recvQ[iFrame].valid = 1;

                            printf("Checksum OK\n");
                            // Last frame recieved
                            if (hdr.flags == FLAG_END_DATA) {
                                // Signal finish
                                td->finish = 1;
                            }

                            tmp[0] = hdr.seqNum;
                            tmp[1] = FLAG_ACK_VALID;
                            clientSend(&tmp[0]);
                            printf("Ack %i sent\n", hdr.seqNum);
        
                        } else {
                            printf("Checksum failed\n");
                            printf("EXP: %X, GOT: %X\n", tmpChk, chk);
                        }
                    }
                }

                pthread_mutex_unlock(&td->slide);

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

    fp = fopen("/tmp/WoahWhatATest.txt", "wb");
    td = (thread_data *) data;

    while (!td->finish) {

        // Wait for valid slot to open
        if (td->ss.recvQ[0].valid) {
            pthread_mutex_lock(&td->slide);
            if (fwrite(td->ss.recvQ[0].msg, MLEN, 1, fp) != 1) {
                perror("write: ");
            }

            for (int c = 0; c < RWS-1; c++) {
                *(td->ss.recvQ[c].msg) = *(td->ss.recvQ[c+1].msg);
                td->ss.recvQ[c].valid = td->ss.recvQ[c+1].valid;
                td->ss.recvQ[c].ackNum = td->ss.recvQ[c+1].ackNum;
            }

            memset(td->ss.recvQ[RWS-1].msg, 0, FULL);
            td->ss.recvQ[RWS-1].valid = 0;

            // Update window size
            td->ss.FFQ = (++td->ss.FFQ) % SN;
            td->ss.LFQ = (++td->ss.LFQ) % SN;

            td->ss.recvQ[RWS-1].ackNum = td->ss.LFQ;
            pthread_mutex_unlock(&td->slide);
            sem_post(&td->full);
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
    td->finish = 0;
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
    servaddr.sin_addr.s_addr = inet_addr(THING1); 

    // Initialize client window values
    for (int i = 0; i < RWS; i++) {
        td->ss.recvQ[i].msg = (char *)calloc(1, FULL);
        td->ss.recvQ[i].valid = 0;
        td->ss.recvQ[i].ackNum = i;
    }
    // Initialize thread stuff
    pthread_mutex_init(&td->slide, NULL);
    sem_init(&td->full, 0, RWS);

}

void clientDestroy (thread_data *td) {

    for (int i = 0; i < RWS; i++) {
        free(td->ss.recvQ[i].msg);
    }

    pthread_mutex_destroy(&td->slide);
    sem_destroy(&td->full);

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
