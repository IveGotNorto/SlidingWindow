
#include "utilities.h"

int sockfd, tOut; 
pthread_t threads[NUM_THREADS];
unsigned int len;
struct sockaddr_in servaddr, cliaddr; 

typedef struct {
    int dAck[SN];
    int chk[SN];
    int dPack[SN];
    int dAckSize;
    int chkSize;
    int dPackSize;
} errors;

typedef struct {
    serv_swp_state ss;
    errors err;
    mutex slide;
    mutex send;
    semaphore empty;
    int conn;
    int rttBool;
    time_t startTime;
    int numPackets;
    int droppedPackets;
    double fileSize;
} thread_data;

// Send completed frame to client
void serverSend (char *frame, size_t fSize) {
    len = sizeof(cliaddr);
    if ( sendto(sockfd, (const char *)frame, fSize, 
                MSG_WAITALL, (const struct sockaddr *)&cliaddr, 
                len) == -1) {
        perror("sendto: ");
    }
}

void *serverListen (void *data) {

    int i;
    int expectedAck = 0;
    char buffer[HLEN];
    thread_data *td;
    swp_hdr hdr;
    td = (thread_data *) data; 
    len = sizeof(cliaddr);

    while (1) {
        // Get ACKS
        if (recvfrom(sockfd, (char *)buffer, HLEN,  
                     MSG_WAITALL, (struct sockaddr *) &cliaddr, 
                     &len) != -1
            ) {
            // Strip off header
            memcpy(&hdr, &buffer, sizeof(swp_hdr));
            
            if (hdr.flags == FLAG_ACK_VALID) {
                #ifdef SaW
		// Verify ACKs for Stop and Wait
                if(hdr.seqNum == td->ss.LFQ) {
	            // Flip the ACK bit for the last sent frame
                    td->ss.sendQ[td->ss.LFQ].ack = 1;
                    printf("Ack %i recieved\n", hdr.seqNum);
                    // Unlock the mutex to allow our writer to write again
                    pthread_mutex_unlock(&td->slide);
                }
                #else
                pthread_mutex_lock(&td->slide);
                if (swpInWindow(hdr.seqNum, td->ss.FFQ, td->ss.LFQ)) {
                            
                    i = 0;
                    // Search for sequence number
                    while (hdr.seqNum != td->ss.sendQ[i].msg[0]) {
                        i = (++i) % SWS;
                    }

                    // Check for dropping ack
                    if ( ERRORCHK( ERROR_DROP_ACK, td->ss.sendQ[i].errors )) {
                        // Get rid of error
                        td->ss.sendQ[i].errors = 0;
                    } else {
                        #ifdef GBN
                        // Check if the ACK sent by client is out of order, and unACKed
                        if(expectedAck != hdr.seqNum && !td->ss.sendQ[i].ack) {
                            printf("Ack %i received out of order. ACKing all packets with a lower sequence number.\n", hdr.seqNum);
                            int k = 0;
                            int j = hdr.seqNum;	
                            // Deal with wrap around cases
                            if ((hdr.seqNum - expectedAck) < 0) {
                                k = (SN - expectedAck) + hdr.seqNum;
                            // Normal cases
                            } else {
                                k = hdr.seqNum - expectedAck;
                            }
                            // ACK any frames in the ACK below the ACK
                            // sent by the client
                            for(int p = k + 1; p > 0; p--) {
				// Find the next frame to ACK using the next lowest
				// sequence number
                    		while (j != td->ss.sendQ[i].msg[0]) {
                        	    i = (++i) % SWS;
                    		}

				 if(!td->ss.sendQ[i].ack) {
                                    printf("Acked %i\n", j);
                                    td->ss.sendQ[i].ack = 1;
                                }	
                                j--; 
                                if(j == -1) {
                                    j = SN - 1;	
                                }
				
                            }
                            // Update our next expected ACK
                            expectedAck = (++hdr.seqNum) % SN;
                        } else {
                            // This is a weird check that is necessary for catching
                            // A race condition for when two ACK's for the same value
                            // are sent back by the client
                            if(expectedAck == hdr.seqNum && !td->ss.sendQ[i].ack) {
                                // ACK and increment expectedACK.
                                printf("Ack %i recieved\n", hdr.seqNum);
                                td->ss.sendQ[i].ack = 1;
                                expectedAck = (++expectedAck) % SN;
                            }
                        }
                        #else
                        // Selective Repeat
                        printf("Ack %i recieved\n", hdr.seqNum);
                        td->ss.sendQ[i].ack = 1;
                        #endif
                    } 
                }
                pthread_mutex_unlock(&td->slide);
                #endif
            } else if (hdr.flags == FLAG_CLIENT_JOIN) {
                // Send the same message back to client
                pthread_mutex_lock(&td->send);
                td->conn = 1;
                serverSend(&buffer[0], HLEN);
                pthread_mutex_unlock(&td->send);
            } else if (hdr.flags == FLAG_CLIENT_EXIT) {
                // Client is leaving -- transmission done
                pthread_mutex_lock(&td->send);
                td->conn = 0;
                serverSend(&buffer[0], HLEN);
                pthread_mutex_unlock(&td->send);
            } else if(hdr.flags == FLAG_CLIENT_RTT) {
		td->rttBool = 1;
	    }

        } else {
            perror("recvfrom:");
        }

    }

    return NULL;

}

void *serverTimer (void *data) {

    int i;
    timeout tmp;
    thread_data *td;
    td = (thread_data *) data; 

    i = 0;
    
    while (td->conn) {

        #ifndef SaW
        // Check if a slide can happen
        if (td->ss.sendQ[0].ack) {
            pthread_mutex_lock(&td->slide);
            for (int c = 0; c < SWS - 1; c++) {
                memcpy(td->ss.sendQ[c].msg, td->ss.sendQ[c+1].msg, FULL);
                td->ss.sendQ[c].start = td->ss.sendQ[c+1].start;
                td->ss.sendQ[c].size = td->ss.sendQ[c+1].size;
                td->ss.sendQ[c].ack = td->ss.sendQ[c+1].ack;
                td->ss.sendQ[c].errors = td->ss.sendQ[c+1].errors;
            }

            // Reset values of last array position
            memset(td->ss.sendQ[SWS-1].msg, 0, FULL);
            td->ss.sendQ[SWS-1].start.tv_sec = 0;
            td->ss.sendQ[SWS-1].start.tv_usec = 0;
            td->ss.sendQ[SWS-1].size = 0;
            td->ss.sendQ[SWS-1].ack = 0;
            td->ss.sendQ[SWS-1].errors = 0;

            // Update the Frame Index
            td->ss.FI--;             
            // Update the First Frame Queued
            td->ss.FFQ = (++td->ss.FFQ) % SN;

            pthread_mutex_unlock(&td->slide);
            // Signal writer there's a spot open
            sem_post(&td->empty);
        }
        #endif

        // See if there is a valid frame in the window
        if (!td->ss.sendQ[i].ack &&
            (td->ss.sendQ[i].start.tv_sec != 0 &&
            td->ss.sendQ[i].start.tv_usec != 0)
            ) {
            // Check timeout 
            gettimeofday(&tmp, NULL);
            // Timeout occurs here 
            if (timeDiff(td->ss.sendQ[i].start, tmp) > tOut) {
                td->droppedPackets = td->droppedPackets + 1;
                if ( ERRORCHK( ERROR_BAD_CHK, td->ss.sendQ[i].errors )) {
                    pthread_mutex_lock(&td->send);
                    printf("Packet %i ***** Timed Out *****\n", td->ss.sendQ[i].msg[0]);
                    serverSend(td->ss.sendQ[i].msg, FULL-2);
                    printf("Packet %i Re-transmitted.\n", td->ss.sendQ[i].msg[0]);
                    pthread_mutex_unlock(&td->send);
                    td->ss.sendQ[i].errors ^= ERROR_BAD_CHK;
                } else {
                    // initial send
                    pthread_mutex_lock(&td->send);
                    printf("Packet %i ***** Timed Out *****\n", td->ss.sendQ[i].msg[0]);
                    serverSend(td->ss.sendQ[i].msg, FULL);
                    printf("Packet %i Re-transmitted.\n", td->ss.sendQ[i].msg[0]);
                    pthread_mutex_unlock(&td->send);
                }
                // Update time
                gettimeofday(&td->ss.sendQ[i].start, NULL);
            }
        }
            
        #ifdef SaW
        i = td->ss.LFQ;
        #else
        i = (++i) % SWS;
        usleep(200);
        #endif

    }

    return NULL;

}

void *serverWriter (void *data) {

    int k;    
    int i;
    int chkErrI, dPackErrI, dAckErrI;
    FILE *fp;
    swp_hdr hdr;
    thread_data *td;
    uint16 fLeft;
    uint64 fSize, fIter;
    char buffer[FULL], cont[MLEN];

    swp_hdr tmpHdr;
    td = (thread_data *) data; 
    fp = getFname("rb", &buffer[0]);
    fSize = fileSize(fp);
    td->fileSize = fSize;
    fLeft = fSize % MLEN;
    fIter = fSize / MLEN;
    time(&td->startTime);
    chkErrI = 0;
    dPackErrI = 0;
    dAckErrI = 0;
    i = 0;

    memset(&buffer[0], 0, FULL);

    while (i <= fIter && td->conn) {
        #if defined SaW
        // Check if the last frame queue was a 1 or 0
        if(td->ss.LFQ || i == 0) {
            td->ss.LFQ = 0;
            hdr.seqNum = 0;
        } else {
            td->ss.LFQ = 1;
            hdr.seqNum = 1;
        }

        // Check if we are have hit our last frame
        // And set the appropriate flag
        // Fill the packet with the appropriate number of bytes	
        if (i == fIter) {
            // last frame
            hdr.flags = FLAG_END_DATA; 
            fread(&cont, 1, fLeft, fp);
            hdr.size = fLeft;
            td->numPackets = i + 1;
        } else {
            hdr.flags = FLAG_HAS_DATA; 
            fread(&cont, 1, MLEN, fp);
            hdr.size = MLEN;
        }

        createFrame(td->ss.sendQ[td->ss.LFQ].msg, hdr, &cont[0]);
    
        // initial send
        pthread_mutex_lock(&td->send);
        serverSend(td->ss.sendQ[td->ss.LFQ].msg, FULL);
        pthread_mutex_unlock(&td->send);
        printf("Packet %i sent.\n", hdr.seqNum);
        gettimeofday(&td->ss.sendQ[td->ss.LFQ].start, NULL);
        i++;

        #else

        // Check if array is empty
        sem_wait(&td->empty);    
        pthread_mutex_lock(&td->slide);
        // Attach flags for frame
        hdr.seqNum = td->ss.LFQ;

        if (i == fIter) {
            hdr.flags = FLAG_END_DATA; 
            fread(&cont, 1, fLeft, fp);
            hdr.size = fLeft;
            td->numPackets = i + 1;
        } else {
            hdr.flags = FLAG_HAS_DATA; 
            fread(&cont, 1, MLEN, fp);
            hdr.size = MLEN;
        }

        // checksum is attached in createFrame
        createFrame(td->ss.sendQ[td->ss.FI].msg, hdr, &cont[0]);
        
        // Dropped Acks
        if (td->err.dAckSize) {
            if (hdr.seqNum == td->err.dAck[dAckErrI]) {
                td->ss.sendQ[td->ss.FI].errors |= ERROR_DROP_ACK;
            }
            if (hdr.seqNum >= td->err.dAck[dAckErrI] &&
                hdr.seqNum <= td->err.dAck[td->err.dAckSize - 1]) {
                dAckErrI = (++dAckErrI) % td->err.dAckSize;
            }
        }
        // Dropped Packets
        if (td->err.dPackSize) {
            if (hdr.seqNum == td->err.dPack[dPackErrI]) {
                td->ss.sendQ[td->ss.FI].errors |= ERROR_DROP_PACK;
            }
            if (hdr.seqNum >= td->err.dPack[dPackErrI] &&
                hdr.seqNum <= td->err.dPack[td->err.dPackSize - 1]) {
                dPackErrI = (++dPackErrI) % td->err.dPackSize;
            }
        }
        // Bad Checksums
        if (td->err.chkSize) {
            if (hdr.seqNum == td->err.chk[chkErrI]) {
                td->ss.sendQ[td->ss.FI].errors |= ERROR_BAD_CHK;
            }
            if (hdr.seqNum >= td->err.chk[chkErrI] &&
                hdr.seqNum <= td->err.chk[td->err.chkSize - 1]) {
                chkErrI = (++chkErrI) % td->err.chkSize;
            }
        }

        if ( ERRORCHK( ERROR_DROP_PACK, td->ss.sendQ[td->ss.FI].errors )) {
            // Do nothing
        } else if ( ERRORCHK( ERROR_BAD_CHK, td->ss.sendQ[td->ss.FI].errors )) {
            pthread_mutex_lock(&td->send);
            serverSend(td->ss.sendQ[td->ss.FI].msg, FULL-2);
            pthread_mutex_unlock(&td->send);
            printf("Packet %i sent\n", hdr.seqNum);
        } else {
            // initial send
            pthread_mutex_lock(&td->send);
            serverSend(td->ss.sendQ[td->ss.FI].msg, FULL);
            pthread_mutex_unlock(&td->send);
            printf("Packet %i sent\n", hdr.seqNum);
        }

        gettimeofday(&td->ss.sendQ[td->ss.FI].start, NULL);
        td->ss.LFQ = (++td->ss.LFQ) % SN;
        ++td->ss.FI;    // Frame Index will never go above SWS - 1 

        pthread_mutex_unlock(&td->slide);

        memset(&buffer[0], 0, FULL);
        memset(&cont[0], 0, MLEN);
        i++;
	    
        // Print current window size
        if(i < 8) {	
            printf("Current window = [0,1,2,3,4,5,6,7]\n");	
        } else {
            printf("Current window = [");
            // Pull last frame value
            int t = td->ss.LFQ - SWS;
            // Find if we need to wrap around due our window going from the end
            // of SN to the start.
            if(t < 0) {
                t = SN + t;
            }
            // Print our values, make sure to check for 
            // wrapping and deal with it
            for(int k = 0; k < SWS - 1; k++) {
                printf("%d,", t);
                t++;
                if(t == SN) {
                    t = 0;	
                }
            }
            if(t == SN) {
                t = 0;
            }

            printf("%d]\n", t);
        }
        #endif
    }

    fclose(fp);
    return NULL;

}

void serverDestroy (thread_data *td) {
    // Calculate total transmission time
    time_t endTime;
    time(&endTime);
    double elapsedTime = (double)(endTime - td->startTime);
    
    // Prevent division by 0
    if(!elapsedTime) {
	elapsedTime = 1;
    }

    for (int i = 0; i < SWS; i++) {
        free(td->ss.sendQ[i].msg);
    }

    pthread_mutex_destroy(&td->send);
    pthread_mutex_destroy(&td->slide);
    sem_destroy(&td->empty);
    close(sockfd);

    printf("Session successfully terminated\n");
    printf("Number of original packets sent: %d\n", td->numPackets);
    printf("Number of retransmitted packets: %d\n", td->droppedPackets);
    printf("Total elapsed time: %lf seconds\n", elapsedTime);
    printf("Total throughput (Mbps): %lf\n", (((double)((td->fileSize + (td->droppedPackets * FULL)) * 8) / elapsedTime) / 1000000));
    printf("Effective throughput (Mbps): %lf\n", (((double)(td->fileSize * 8) / elapsedTime) / 1000000));
}


int calculateCustomTimeout(thread_data *td) {
    // Create variables necessary for calculation and timing
    float t = 0.125;
    struct timeval start, end, check;
    double rttm, rtts = 0;
   
    // Create our test packet to send
    char testPacket[FULL];
    swp_hdr rttHdr;
    rttHdr.flags = FLAG_SERVER_RTT;
    char rttFrame[FULL];
    rttFrame[1] = rttHdr.flags;
    createFrame(rttFrame, rttHdr, testPacket);
    td->rttBool = 0;
   
    // Loop through and send 50 packets to the client to measure RTT.
    // Perform the calculation to find a smoothed RTT 
    for(int i = 0; i < 10; i++) {
	// Get current time
	gettimeofday(&start, NULL);
	// Send the packet	
	pthread_mutex_lock(&td->send);
	serverSend(&rttFrame[0], FULL);
	pthread_mutex_unlock(&td->send);
	// Wait for the response from the client
    	while(td->rttBool == 0){
	    // Check to see if the response comes
	    // If it does not (Lost ack, dropped packet, bad checksum, etc.)
	    // Then break the while loop
	    gettimeofday(&check, NULL);
 	    if((check.tv_usec - start.tv_usec) > MAXTIMEOUT) {
	        printf("RTT packet has reached tOut. Network traffic may be too high.\n");
		break;
	    }	
            usleep(100);
        }
	// Check if a response was received, if it was perform 
	// smooted RTT cacluations
	// If a response was not received then ignore as this
	// will throw off our RTT calculation
	if(td->rttBool == 1) {
	    if(rtts == 0) {
		gettimeofday(&end, NULL);
		rttm = (end.tv_usec - start.tv_usec);
		rtts = rttm;
		rtts = (1-t)*rtts + t*rttm;
	    } else {
	        gettimeofday(&end, NULL);
       	        rttm = (end.tv_usec - start.tv_usec);
	        rtts = (1-t)*rtts + t*rttm;
	    }
	}
	td->rttBool = 0;
    }
    return (int) rtts * 2;
}

void menu(thread_data *td) {

    printf("\n#####   SERVER (%s)   #####\n", inet_ntoa(servaddr.sin_addr));

    if (!td->conn) {
        printf("\nClient not connected\n");
        while(!td->conn);
    } else {
        printf("\n");
    }

    printf("Client (%s) connected\n", inet_ntoa(cliaddr.sin_addr));

    int t, i;
    char *p, buffer[FULL];

    // Find if user wants to define tOut, and if so set it.
    printf("\nDo you wish to set a timeout, or use a ping-calculated timeout?\n");
    printf("1)Set a timeout\n2)Ping-calculated timeout\n");
    while (scanf("%i", &t) != 1 && ((t != 1) && (t != 2))) {
        getchar(); 
    }
    
    if(t == 1) {	
        printf("\nInput a timeout value (In Microseconds): ");
        while (scanf("%i", &t) && ((t <= 0) || (t > MAXTIMEOUT))) {
            getchar(); 
        }
    } else {
        t = calculateCustomTimeout(td);
        printf("\nCalculated timeout using given packet size of %d bytes is: %d microseconds\n", FULL, t);
    }

    // Assign timeout
    tOut = t;

    printf("\nDo you wish to create errors?\n");
    printf("1)Yes\n2)No\n");
    while (scanf("%i", &t) != 1 && ((t != 1) && (t != 2))) {
        getchar(); 
    }
 
    if (t == 1) {
        while (t != 0) {
            memset(&buffer, 0, SN);

            printf("\n1)Drop Packets\n");
            printf("2)Bad Checksums\n");
            printf("3)Drop Acks\n");
            printf("4)Randomize\n");
            printf("0)Done\n");
            printf("Input: ");

            scanf("%i", &t);

            if (t == 4) {
                // Set randomness
            }

            if (t > 0 && t < 5) {
                printf("\nEnter sequence numbers(,): ");
                if (t == 1) {
                    memset(&(td->err.dPack), 0, SN);
                    scanf("%s", &buffer[0]);
                } else if (t == 2) {
                    memset(&(td->err.chk), 0, SN);
                    scanf("%s", &buffer[0]);
                } else if (t == 3) {
                    memset(&(td->err.dAck), 0, SN);
                    scanf("%s", &buffer[0]);
                }

                i = 0;
                p = strtok(buffer,"- ,");

                while (p != NULL) {
                    buffer[i] = atoi(p);        
                    if (t == 1) {
                        td->err.dPack[i] = buffer[i];
                    } else if (t == 2) {
                        td->err.chk[i] = buffer[i];
                    } else if (t == 3) {
                        td->err.dAck[i] = buffer[i];
                    }
                    p = strtok(NULL,"- ,");
                    i++;
                }

                if (t == 1) {
                    td->err.dPackSize = i;
                } else if (t == 2) {
                    td->err.chkSize = i;
                } else if (t == 3) {
                    td->err.dAckSize = i;
                } 
            }
        }
    }
}


void serverInit (thread_data *td) {
    
    int opt = 1;

    // Connection
    td->conn = 0;
    // Window
    td->ss.FI = 0;
    td->ss.FFQ = 0;
    td->ss.LFQ = 0;
    // Errors
    td->err.dPackSize = 0;
    td->err.dAckSize = 0;
    td->err.chkSize = 0;

    td->droppedPackets = 0;
	
    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE);
    } 
                                   
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    // Server information 
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORTA); 
    // Set server address
    servaddr.sin_addr.s_addr = inet_addr(SERVER); 

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Initialize all values of the window
    for (int i = 0; i < SWS; i++) {
        td->ss.sendQ[i].msg = (char *)malloc(FULL);
        td->ss.sendQ[i].start.tv_sec = 0;
        td->ss.sendQ[i].start.tv_usec = 0;
        td->ss.sendQ[i].errors = 0;
        td->ss.sendQ[i].size = 0;
        td->ss.sendQ[i].ack = 0;
    }

    // Initialize thread stuff
    pthread_mutex_init(&td->slide, NULL);
    pthread_mutex_init(&td->send, NULL);
    sem_init(&td->empty, 0, SWS);
}

int main (void) {

    int n;
    thread_data td; 
    serverInit(&td);

    // Create listener
    n = pthread_create(&threads[0], NULL, serverListen, (void *)&td);
    if (n) {
        printf("Error: Unable to create listener thread.\n");
        exit(-1);
    }

    menu(&td);

    // Create writer
    n = pthread_create(&threads[1], NULL, serverWriter, (void *)&td);
    if (n) {
        printf("Error: Unable to create writer thread.\n");
        exit(-1);
    }

    // Create timer
    n = pthread_create(&threads[2], NULL, serverTimer, (void *)&td);
    if (n) {
       printf("Error: Unable to create timer thread.\n");
       exit(-1);
    }

    pthread_join(threads[2],NULL);
    serverDestroy(&td);
    return 0;
}

