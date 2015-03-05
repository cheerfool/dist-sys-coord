#ifndef TOOLS_H_
#define TOOLS_H_ 

#include <stdbool.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>
#include "msg.h"

int gsize;

void usage(char *cmd); 

void die(char *msg);

void dieByUser(char *msg);

int readGroup(char *fileName, char hosts[][32], char ids[][8], int maxSize);

void idToPort(char ids[][8], unsigned long ports[], int gsize);

int checkGroup(unsigned long port, unsigned long ports[], int gsize);

void copyClock(struct clock newClock[], struct clock ownClock[]);

void updateClock(struct clock ownClock[], struct clock newClock[]);

void logClock(FILE *fp, struct clock myVectorClock[], unsigned long port);

char *msgTypeString(msgType type);

unsigned int resolveSocketAddress(const struct sockaddr *address, FILE *stream, bool printing);

unsigned int getSocketPort(const struct sockaddr *address, bool printing);

#endif 
