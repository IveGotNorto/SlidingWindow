#ifndef ___UTILITIES_H___
#define ___UTILITIES_H___

#include "crc.h"
#include <cstdio> 
#include <cstdlib> 
#include <cstring> 
#include <cmath>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>

// Port Declarations
#define PORTA 9012
#define PORTB 9113

// Packet Information
#define HLEN 4          // Frame header length
#define CLEN 4          // Checksum length
#define MLEN 1492       // Frame message length
#define FULL 1500       // Total frame size

// Misc. Tweaks
#define NUM_THREADS 5
#define ATTEMPTS 10
#define MAXTIMEOUT 1000    // Max timeout used in RTT ping calculation

// Frame Flag Definitions
#define FLAG_ACK_VALID 0x1
#define FLAG_HAS_DATA 0x2
#define FLAG_END_DATA 0x3
#define FLAG_CLIENT_JOIN 0x4
#define FLAG_SERVER_RTT 0x5
#define FLAG_CLIENT_EXIT 0x6
#define FLAG_CLIENT_RTT 0x7

// Error Flag Definitions
#define ERROR_DROP_PACK 0x1
#define ERROR_DROP_ACK 0x2
#define ERROR_BAD_CHK 0x4

// Define the server
//#define SERVER "10.35.195.46"   // THING 0
#define SERVER "10.35.195.47"   // THING 1
//#define SERVER "10.35.195.22"   // THING 2
//#define SERVER "10.35.195.49"   // THING 3

// Set Protocol
//#define SaW
//#define GBN
#define SR

#define ERRORCHK(a,b) a == (b & a)

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long uint64;

typedef pthread_mutex_t mutex;
typedef pthread_cond_t condition;
typedef struct timeval timeout;
typedef sem_t semaphore;

const uint32 SWS = 8;   // Server window size
const uint32 RWS = 8;   // Recieving window size
const uint32 SN = 32;   // Sequence Number

typedef char* Msg;

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
        uint8 errors;
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
    } recvQ[RWS];

} cli_swp_state;

FILE *getFname (const char *method, char *fileName);

long fileSize (FILE *fp);

void createFrame (char *frame, swp_hdr hdr, char *buffer);

unsigned long timeDiff (timeout before, timeout after);

int swpInWindow (uint8 seqNum, uint8 min, uint8 max);

#endif
