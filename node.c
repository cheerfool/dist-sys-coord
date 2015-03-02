#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <stdio.h>
#include <errno.h>

#include "msg.h"
#include "tools.h"

int gsize;
char hosts[9][64];
unsigned long ports[9];

//void usage(char * cmd) 
//void die(char *msg)
//void dieF(char *msg)
//int readGroup(char* fileName, char[][] hosts, unsigned long[] ports)
//void checkGroup(unsigned long port, unsigned long[] ports)

int main(int argc, char ** argv) {

  // This is some sample code feel free to delete it
  
  unsigned long  port;
  char *         groupListFileName;
  char *         logFileName;
  unsigned long  timeoutValue;
  unsigned long  AYATime;
  unsigned long  myClock = 1;
  unsigned long  sendFailureProbability;
  if (argc != 7) {
    usage(argv[0]);
    return -1;
}
  
  char * end;
  int err = 0;

  port = strtoul(argv[1], &end, 10);
  if (argv[1] == end) {
    printf("Port conversion error\n");
    err++;
  }

  groupListFileName = argv[2];
  logFileName       = argv[3];

  gsize= readGroup(groupListFileName, hosts, ports);
  checkGroup(port, ports, gsize);

  FILE *logf= fopen(logFileName, "w+");
  if(logf==NULL)
	die("Can not write to the log file.");
  fprintf(logf, "starting N%lu\n", port);

  timeoutValue      = strtoul(argv[4], &end, 10);
  if (argv[4] == end) {
    printf("Timeout value conversion error\n");
    err++;
  }

  AYATime  = strtoul(argv[5], &end, 10);
  if (argv[5] == end) {
    printf("AYATime conversion error\n");
    err++;
  }

  sendFailureProbability  = strtoul(argv[6], &end, 10);
  if (argv[6] == end) {
    printf("sendFailureProbability conversion error\n");
    err++;
  }

  printf("Port number:              %d\n", port);
  printf("Group list file name:     %s\n", groupListFileName);
  printf("Log file name:            %s\n", logFileName);
  printf("Timeout value:            %d\n", timeoutValue);  
  printf("AYATime:                  %d\n", AYATime);
  printf("Send failure probability: %d\n", sendFailureProbability);
  printf("Starting up Node %d\n", port);
  
  printf("N%d {\"N%d\" : %d }\n", port, port, myClock++);
  printf("Sending to Node 1\n");
  printf("N%d {\"N%d\" : %d }\n", port, port, myClock++);
  
  if (err) {
    printf("%d conversion error%sencountered, program exiting.\n",
	   err, err>1? "s were ": " was ");
    return -1;
  }


  // If you want to produce a repeatable sequence of "random" numbers
  // replace the call time() with an integer.
  //srandom(time());
  srandom(510);
  
  int i;
  for (i = 0; i < 10; i++) {
    int rn;
    rn = random(); 

    // scale to number between 0 and the 2*AYA time so that 
    // the average value for the timeout is AYA time.

    int sc = rn % (2*AYATime);
    printf("Random number %d is: %d\n", i, sc);
  }

  fclose(logf);
  return 0;
  
}
