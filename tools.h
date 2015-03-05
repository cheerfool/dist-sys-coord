#ifndef TOOLS_H_
#define TOOLS_H_ 

#include <stdbool.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>

void usage(char *cmd); 

void die(char *msg);

void dieF(char *msg);

int readGroup(char *fileName, char hosts[][32], char ids[][8], int maxSize);

void idToPort(char ids[][8], unsigned long ports[], int gsize);

int checkGroup(unsigned long port, unsigned long ports[], int gsize);

unsigned int PrintSocketAddress(const struct sockaddr *address, FILE *stream);

unsigned int getSocketPort(const struct sockaddr *address);

#endif 
