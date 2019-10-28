#ifndef ___UTILITIES_H___
#define ___UTILITIES_H___

#include <netdb.h> 
#include <cstdio> 
#include <cstdlib> 
#include <string> 
#include <cstring> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "blowfish.h"

#define SA struct sockaddr 
#define PORTB 9251
#define PORTKDC 9241
#define BSIZE 1024   // Block size in bytes
#define MAX 56

#define DEBUG 0

using namespace std;

typedef unsigned char uint8;

const string ip_thing0 = "10.35.195.46";
const string ip_thing1 = "10.35.195.47";
const string ip_thing2 = "10.35.195.22";
//const string ip_thing2 = "10.35.195.49";

FILE *getFname (const char *method, char *fileName);

long f (long nonce);

char *recieve_all (int sockfd, long *size);

void send_all (int sockfd, char *buf, long *len);

void encrypt (Blowfish bf, long *size, long tsize, char *cont);

void decrypt (Blowfish bf, long *size, char *cont);

void sender (int sockfd, void *cont, long size);

int reciever (int sockfd, void *cont, long size);

long fileSize (FILE *fp);

#endif
