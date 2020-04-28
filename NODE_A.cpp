#include "utilities.h"

int params[4];
int sockfd, conn; 
pthread_t threads[NUM_THREADS];
unsigned int len;
struct sockaddr_in servaddr, cliaddr; 

typedef struct {
    serv_swp_state ss;
    mutex slide;
    mutex send;
    semaphore empty;
    bool rttBool;
    time_t startTime;
    int numPackets;
    int droppedPackets;
    double fileSize;
} thread_data;

// Send completed frame to client
void serverSend (char *frame) {
    len = sizeof(cliaddr);
    if ( sendto(sockfd, (const char *)frame, FULL, 
                MSG_WAITALL, (const struct sockaddr *)&cliaddr, 
                len) == -1) {
        perror("sendto: ");
    }
}

void *serverListen (void *data) {

    int i;
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


		if(params[0] == 1) {
		    if(hdr.seqNum == td->ss.LFQ) {
		    	td->ss.sendQ[td->ss.LFQ].ack = 1;
                    	printf("Ack %i recieved\n", hdr.seqNum);
		    	pthread_mutex_unlock(&td->slide);
		    }
		} else if(params[0] == 2) {

		} else {
		    // Selective Repeat
                    pthread_mutex_lock(&td->slide);
                    if (swpInWindow(hdr.seqNum, td->ss.FFQ, td->ss.LFQ)) {

                    	i = 0;
                    	// Search for sequence number
                    	while (hdr.seqNum != td->ss.sendQ[i].msg[0]) {
                            i = (++i) % SWS;
                    	}
                    	printf("Ack %i recieved\n", hdr.seqNum);
                    	td->ss.sendQ[i].ack = 1;

                    }
                    pthread_mutex_unlock(&td->slide);
		}
            } else if (hdr.flags == FLAG_CLIENT_JOIN) {
                // Send the same message back to client
                pthread_mutex_lock(&td->send);
                conn = 1;
                serverSend(&buffer[0]);
                pthread_mutex_unlock(&td->send);
            } else if (hdr.flags == FLAG_CLIENT_EXIT) {
                // Client is leaving -- transmission done
                pthread_mutex_lock(&td->send);
                conn = 0;
                serverSend(&buffer[0]);
                pthread_mutex_unlock(&td->send);
            } else if(hdr.flags == FLAG_CLIENT_RTT) {
		td -> rttBool = 1;
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
    
    while (conn) {

            // Check if a slide can happen
            if (td->ss.sendQ[0].ack && params[0] == 3) {
                pthread_mutex_lock(&td->slide);
                for (int c = 0; c < SWS - 1; c++) {
                    memcpy(td->ss.sendQ[c].msg, td->ss.sendQ[c+1].msg, FULL);
                    td->ss.sendQ[c].start = td->ss.sendQ[c+1].start;
                    td->ss.sendQ[c].size = td->ss.sendQ[c+1].size;
                    td->ss.sendQ[c].ack = td->ss.sendQ[c+1].ack;
                }

                // Reset values of last array position
                memset(td->ss.sendQ[SWS-1].msg, 0, FULL);
                td->ss.sendQ[SWS-1].start.tv_sec = 0;
                td->ss.sendQ[SWS-1].start.tv_usec = 0;
                td->ss.sendQ[SWS-1].size = 0;
                td->ss.sendQ[SWS-1].ack = 0;

                // Update the Frame Index
                td->ss.FI--;             
                // Update the First Frame Queued
                td->ss.FFQ = (++td->ss.FFQ) % SN;

                pthread_mutex_unlock(&td->slide);
                // Signal writer there's a spot open
                sem_post(&td->empty);
            }
        

            // See if there is a valid frame in the window
            if (td->ss.sendQ[i].msg[HLEN] != 0 &&
		!td->ss.sendQ[i].ack &&
            	(td->ss.sendQ[i].start.tv_sec != 0 ||
            	td->ss.sendQ[i].start.tv_usec != 0)
            	) {
            	// Check timeout 
            	gettimeofday(&tmp, NULL);
            	// Timeout occurs here 
           	if (timeDiff(td->ss.sendQ[i].start, tmp) > params[2]) {
		    td->droppedPackets = td->droppedPackets + 1;
		    pthread_mutex_lock(&td->send);
                    printf("Packet %i ***** Timed Out *****\n", td->ss.sendQ[i].msg[0]);
                    serverSend(td->ss.sendQ[i].msg);
                    printf("Packet %i Re-transmitted.\n", td->ss.sendQ[i].msg[0]);
                    pthread_mutex_unlock(&td->send);
                    // Update time
                    gettimeofday(&td->ss.sendQ[i].start, NULL);
            	}
		
		if(params[0] == 1) {
		    i = td->ss.LFQ;
		} else if(params[0] == 2) {

		} else {
		    i = (++i) % SWS;
		}
            	usleep(20);
            }
    }

    return NULL;

}

void *serverWriter (void *data) {
    
    int i;
    FILE *fp;
    swp_hdr hdr;
    thread_data *td;
    uint16 fLeft;
    uint64 fSize, fIter;
    char buffer[params[1]], cont[params[1]];

    td = (thread_data *) data; 
    fp = getFname("rb", &buffer[0]);
    fSize = fileSize(fp);
    td->fileSize = fSize;
    printf("Filesize: %d\n", fSize);
    fLeft = fSize % params[1];
    fIter = fSize / params[1];
    time(&td->startTime);

    while (i <= fIter && conn) {
        if(params[0] == 1) {
            pthread_mutex_lock(&td->slide);
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
		hdr.size = htons(fLeft);
		td->numPackets = i + 1;
            } else {
                hdr.flags = FLAG_HAS_DATA; 
                fread(&cont, 1, params[1], fp);
		hdr.size = htons(params[1]);
            }

            createFrame(td->ss.sendQ[td->ss.LFQ].msg, hdr, &cont[0]);
        
            // do initial send
            pthread_mutex_lock(&td->send);
            serverSend( td->ss.sendQ[td->ss.LFQ].msg );
            pthread_mutex_unlock(&td->send);
            printf("Packet %i sent.\n", hdr.seqNum);
            gettimeofday(&td->ss.sendQ[td->ss.LFQ].start, NULL);
            i++;
        } else if(params[0] == 2) {

        } else {
            // Check if array is empty
            sem_wait(&td->empty);    
            pthread_mutex_lock(&td->slide);
            // Attach flags for frame
            hdr.seqNum = td->ss.LFQ;

            if (i == fIter) {
                hdr.flags = FLAG_END_DATA; 
                fread(&cont, 1, fLeft, fp);
                hdr.size = htons(fLeft);
		td->numPackets = i + 1;
            } else {
                hdr.flags = FLAG_HAS_DATA; 
                fread(&cont, 1, params[1], fp);
                hdr.size = htons(params[1]);
            }

            // Checksum is attached in createFrame
            createFrame(td->ss.sendQ[td->ss.FI].msg, hdr, &cont[0]);

            // do initial send
            pthread_mutex_lock(&td->send);
            serverSend( td->ss.sendQ[td->ss.FI].msg );
            pthread_mutex_unlock(&td->send);

            printf("Packet %i sent\n", hdr.seqNum);
            gettimeofday(&td->ss.sendQ[td->ss.FI].start, NULL);
            td->ss.LFQ = (++td->ss.LFQ) % SN;
            ++td->ss.FI;    // Frame Index will never go above SWS - 1 

            pthread_mutex_unlock( &td->slide );

            memset(&cont[0], 0, params[1]);
            i++;
        }
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
    printf("Total throughput (Mbps): %lf\n", ((((td->fileSize + (double)(td->droppedPackets * params[1])) * 8) / elapsedTime) / 1000000));
    printf("Effective throughput (Mbps): %lf\n", (((td->fileSize * 8) / elapsedTime) / 1000000));
}


int calculateCustomTimeout(thread_data *td) {
    // Create variables necessary for calculation and timing
    float t = 0.125;
    struct timeval start, end, check;
    double rttm, rtts = 0;
   
    // Create our test packet to send
    char testPacket[params[1]];
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
	serverSend(&rttFrame[0]);
	pthread_mutex_unlock(&td->send);
	// Wait for the response from the client
    	while(td->rttBool == 0){
	    // Check to see if the response comes
	    // If it does not (Lost ack, dropped packet, bad checksum, etc.)
	    // Then break the while loop
	    gettimeofday(&check, NULL);
 	    if((check.tv_usec - start.tv_usec) > MAXTIMEOUT) {
	        printf("RTT packet has reached timeout. Network traffic may be too high.\n");
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

    int netProtocol, packetSize, timeout, menu, windowSize, sequenceBegin, sequenceEnd, situationalErrors = 0;	

    // Get settings to be used for file transmission from user
    printf("\nDo you wish to use the default set values or input custom values?\n\n");
    printf("1)Load defaults\n2)Custom Values\n");
    cin >> menu;
    printf("\n");
    if(menu == 1) {
	// Set default values here
	netProtocol = 1;
	packetSize = 1492;
	timeout = 350;
	windowSize = 2000;
	sequenceBegin = 123;
	sequenceEnd = 12345;
	params[1] = packetSize;
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
    	params[1] = packetSize;
	
	printf("Packetsize variable: %d\n", packetSize);
	printf("Packetsize params: %d\n", params[1]);

	if(packetSize < 1) {
	    printf("Invalid packet size. Packet size must be positive. Exiting.\n");
	    exit(-1);	
	}else if(packetSize > 1492) {
	    printf("Invalid packet size. Packet size is greater than MTU of 1492 (Packet header causes MTU to be 1492 bytes). Exiting.\n");
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
	    printf("Input a timeout value (In Microseconds): ");
	    cin >> timeout;
	} else {
	    timeout = calculateCustomTimeout(td);
	    printf("Calculated timeout using given packet size of %d bytes is: %d microseconds\n", packetSize, timeout);
	}

			
		
    } else {
	printf("Invalid input. Exiting.");
	exit(2);
    }

    // Build an array of all params to send the receiver
    params[0] = netProtocol;
    params[2] = timeout;
    params[3] = situationalErrors;

   if(conn){ 
	// Buffers for converting int params array to char array
	char paramBuffer[(sizeof(params) * sizeof(int)) + 8];
	char intBuffer[sizeof(int)];	
	// Build the char array, and insert delimiters
	for(int i = 0; i < sizeof(params) / sizeof(params[0]); i++) {
	    sprintf(intBuffer, "%d", params[i]);
	    strcat(paramBuffer, intBuffer);
	    strcat(paramBuffer, ".");
	}

	// Create our packet of the param array
	swp_hdr paramHdr;
	paramHdr.flags = FLAG_SERVER_PARAMS;
	char paramFrame[sizeof(paramBuffer) + HLEN];
	paramFrame[1] = paramHdr.flags;
	memcpy(&paramFrame[2], paramBuffer, (sizeof(paramBuffer)));
	// Send params
	pthread_mutex_lock(&td->send);
	serverSend(&paramFrame[0]);
	pthread_mutex_unlock(&td->send);
    } else {
	printf("NodeB must be connected first\n.");
	exit(2);
    }
    return;
}


void serverInit (thread_data *td) {
    
    int opt = 1;

    conn = 0;
    td->ss.FI = 0;
    td->ss.FFQ = 0;
    td->ss.LFQ = 0;
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
    servaddr.sin_addr.s_addr = inet_addr(THING0); 

    if ( bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
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

    printf("Connect NodeB now.\n");  
    usleep(100);
    while(!conn);

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

