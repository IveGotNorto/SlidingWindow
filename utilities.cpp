#include "utilities.h"

// Get file name and open file
FILE *getFname(const char *method, char *fileName) {
    FILE *fp = NULL;
    while (!strlen(fileName) || !fp) {
        printf("\nPlease enter a filename: ");
        scanf("%s", fileName);
        fp = fopen(fileName, method);
    }
    return fp;
}

long fileSize (FILE *fp) {
    // Need size of file for dynamic memory allocation
    if (fseek(fp, 0, SEEK_END)) {
        printf("Error: Unable to find end of file\n");
        exit(1);
    }
    long size = ftell(fp);
    printf("File input size: %li (bytes)\n", size);
    // Back to beginning of file;
    fseek(fp, 0L, SEEK_SET);
    return size;
}

unsigned long timeDiff (timeout before, timeout after) {
    unsigned long x_ns, y_ns;
    x_ns = (unsigned long)before.tv_sec + before.tv_usec;
    y_ns = (unsigned long)after.tv_sec + after.tv_usec;
    return (unsigned long)y_ns - x_ns;
}

void createFrame (char *frame, swp_hdr hdr, char *buffer) {
    uint32 chk;
    frame[0] = hdr.seqNum;
    frame[1] = hdr.flags;
    hdr.size = htons(hdr.size);
    memcpy(&frame[2], &hdr.size, sizeof(uint16));
    // Copy message to frame
    memcpy(&frame[HLEN], buffer, MLEN);    
    chk = htonl(crcSlow((const unsigned char *)&frame[0], HLEN + MLEN));
    // Copy checksum to frame
    memcpy(&frame[MLEN + HLEN], &chk, sizeof(CLEN)); 
}

int swpInWindow (uint8 seqNum, uint8 min, uint8 max) {
    // This has to be checked due to the cyclic behavior
    // of the sequence numbers
    if (min > max) {
        return (min <= seqNum) || (seqNum <= max) ? 1 : 0;
    }
    return (min <= seqNum) && (seqNum <= max) ? 1 : 0;
}


void printWindow (uint32 low, uint32 len) {

    int i = 0;
    int upper = len - 1;
    printf("Current window: ["); 
    while (i < upper) {
        printf("%i,", (low + i) % SN);
        i++;
    }
    printf("%i]\n", (low + i) % SN);

}

