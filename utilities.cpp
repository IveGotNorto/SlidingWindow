#include "utilities.h"

// Get file name and open file
FILE *getFname(const char *method, char *fileName) {
    FILE *fp = NULL;

    while (!strlen(fileName) || !fp) {
        printf("\nPlease enter a filename: ");
        scanf("%s", fileName);

        fp = fopen(fileName, method);

        /**
            Cant think of a more elegant way to do this
            I dont want to delete the file before the chance
            to make sure it can be written to.
        **/
        if (!strcmp(method, "a") && fp) {
            fclose(fp);
            remove(fileName);
            fp = fopen(fileName, method);
        }

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
    memcpy(&frame[2], buffer, MLEN);    
    chk = htonl(crcFast((const unsigned char *)&frame[0], MLEN + HLEN));
    memcpy(&frame[MLEN + HLEN], &chk, sizeof(uint32)); 
}

int swpInWindow (uint8 seqNum, uint8 min, uint8 max) {
    // This has to be checked due to the cyclic behavior
    // of the sequence numbers
    if (min > max) {
        return (min <= seqNum) || (seqNum <= max) ? 1 : 0;
    }
    return (min <= seqNum) && (seqNum <= max) ? 1 : 0;
}

