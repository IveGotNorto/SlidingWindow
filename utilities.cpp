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

// Function that we pass the nonce to for authentication verification
long f(long nonce) {
    const long A = 48271;
    const long M = 2147483647;
    const long Q = M/A;
    const long R = M%A;

    static long state = 1;
    long t = A * (state % Q) - R * (state / Q);
    
    if (t > 0)
        state = t;
    else
        state = t + M;
    return (long)(((double) state/M)* nonce);
}

char *recieve_all(int sockfd, long *size) {
    long total = 0;
    long bytesleft;
    int r;

    if (r = recv(sockfd, size, sizeof(*size), 0) == -1) {
        perror("recv size");
        exit(1);
    }

    *size = ntohl(*size);
     
    //printf("REC: %i, SIZE:%i\n", r, *size);
    char *buf = (char *)malloc(*size * sizeof(char));
    bytesleft = *size;

    r = 0;
    while (total < *size) {
        r = recv(sockfd, buf + total, bytesleft, 0);
        if (r == -1) { break; };
        total += r;
        bytesleft -= r;
    }
    return buf;

}

void send_all(int sockfd, char *buf, long *len) {

    long total = 0;
    long bytesleft = *len;
    int sent;

    long temp = htonl(*len);
    
    if (sent = send(sockfd, &temp, sizeof(long), 0) == -1) {
        perror("send length");
    }
//    printf("SENT: %i, SIZE: %li\n", sent, *len);

    sent = 0;
    while (total < *len) {
        sent = send(sockfd, buf + total, bytesleft, 0);
        if (sent == -1) { break; }
        total += sent;
        bytesleft -= sent;
    }
    *len = total;

}

void encrypt(Blowfish bf, long *size, long tsize, char *cont) {

    long dif = *size - tsize;
    // Add padding to end of input
    for (int c = tsize; c < *size; c++) {
            cont[c] = 0x00 + dif;
            /**
            To remove padding, look at the last lines of the file.
            This will give you the number of bytes of padding that
            were added.
            **/
    }

    bf.Encrypt(cont, *size);
}

void decrypt(Blowfish bf, long *size, char *cont) {

    long dif;

    bf.Decrypt(cont, *size);
    // If padding is found, change size
    if ((dif = cont[*size - 1]) < 8) {
            *size -= dif;
    }
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



