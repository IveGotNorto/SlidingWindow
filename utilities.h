#ifndef ___UTILITIES_H___
#define ___UTILITIES_H___

#include "crc.h"
#include <cstdio> 
#include <cstdlib> 
#include <cstring> 
#include <cmath>
#include <string>
#include <unistd.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include<string.h>

#define PORTA 9010
#define PORTB 9011

#define HLEN 4      // Frame header length
#define MLEN 1492   // Frame message length
#define FULL 1500   // Total frame size

#define NUM_THREADS 5
#define ATTEMPTS 10
#define MAXTIMEOUT 1000    // Max timeout used in RTT ping calculation

#define THING0 "10.35.195.46"
#define THING1 "10.35.195.47"
#define THING2 "10.35.195.22"
#define THING3 "10.35.195.22"

#define FLAG_ACK_VALID 0xA
#define FLAG_HAS_DATA 0xB
#define FLAG_END_DATA 0xC
#define FLAG_CLIENT_JOIN 0xD
#define FLAG_SERVER_RTT 0xE
#define FLAG_CLIENT_EXIT 0xF
#define FLAG_SERVER_PARAMS 0x1A
#define FLAG_CLIENT_RTT 0x1B

using namespace std;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long uint64;

typedef pthread_mutex_t mutex;
typedef pthread_cond_t condition;
typedef struct timeval timeout;
typedef sem_t semaphore;

typedef char* Msg;

const uint32 SWS = 8;
const uint32 RWS = 8;
const uint32 SN = 32;

typedef struct {
    uint8 seqNum;       // Sequence number of frame
    uint8 flags;        // Additional 8 bits of flags
    uint16 size;        // Size of data -- not including header and checksum
} swp_hdr;

typedef struct {

    uint32 FI;       // Frame Index
    uint32 FFQ;      // First Frame Queued
    uint32 LFQ;      // Last Frame Queued

    struct sendQ_slot {
        uint16 size;
        uint32 ack;
        Msg msg;
        timeout start;
    } sendQ[SWS];

} serv_swp_state;

typedef struct {

    uint32 FFQ;      // First Frame Queued
    uint32 LFQ;      // Last Frame Queued

    struct recvQ_slot {
        uint8 ackNum;   // Identical to Sequence Number
        uint8 flags;    // Flags for checking data
        uint16 size;    // Write size of data
        uint32 valid;   // Is the message valid?
        Msg msg;        // Data
    } recvQ[SWS];

} cli_swp_state;

FILE *getFname (const char *method, char *fileName);

long fileSize (FILE *fp);

void createFrame (char *frame, swp_hdr hdr, char *buffer);

unsigned long timeDiff (timeout before, timeout after);

int swpInWindow (uint8 seqNum, uint8 min, uint8 max);

#endif
