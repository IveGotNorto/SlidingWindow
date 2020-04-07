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

void sender (int sockfd, void *cont, long size) {

    long total = 0;
    long bytesleft = size;
    int sent;
    
    while (total < size) {
        sent = send(sockfd, cont + total, bytesleft, 0);
        if (sent == -1) { 
            perror("send");
            break; 
        }
        total += sent;
        bytesleft -= sent;
    }
    
}

int reciever (int sockfd, void *cont, long size) {

    long total = 0;
    long bytesleft = size;
    int rec;

    while (total < size) {
        rec = recv(sockfd, cont + total, bytesleft, 0);
        if (rec == -1) { break; };
        total += rec;
        bytesleft -= rec;
    }

    return total;
}

unsigned checksum(void *buffer, size_t len, unsigned int seed) {
      unsigned char *buf = (unsigned char *)buffer;
      size_t i;

      for (i = 0; i < len; ++i)
            seed += (unsigned int)(*buf++);
      return seed;
}




