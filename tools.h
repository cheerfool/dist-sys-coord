#ifndef TOOLS_H_
#define TOOLS_H_ 

#include <stdbool.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>

void usage(char *cmd); 

void die(char *msg);

void dieF(char *msg);

int readGroup(char *fileName, char hosts[][64], unsigned long ports[]);
  
void checkGroup(unsigned long port, unsigned long ports[], int gsize);

#endif 
