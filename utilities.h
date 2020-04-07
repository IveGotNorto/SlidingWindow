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

#define SA struct sockaddr 
#define PORTREC 9251
#define SEED 5934624
#define MAX 56

#define DEBUG 0

using namespace std;

typedef unsigned char uint8;

const string ip_thing0 = "10.35.195.46";
const string ip_thing1 = "10.35.195.47";
const string ip_thing2 = "10.35.195.22";
//const string ip_thing2 = "10.35.195.49";


FILE *getFname (const char *method, char *fileName);

long fileSize (FILE *fp);

void sender (int sockfd, void *cont, long size);

int reciever (int sockfd, void *cont, long size);

unsigned checksum(void *buffer, size_t len, unsigned int seed);

#endif
