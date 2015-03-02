#include <stdbool.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>

void usage(char *cmd) {
  printf("usage: %s  portNum groupFileList logFile timeoutValue averageAYATime failureProbability \n",
	 cmd);
}

void die(char *msg){
  printf("Error: %s\n", msg);
  perror("Detail");
  exit(1);
}

void dieF(char *msg){
  printf("Error: %s\n", msg);
  exit(1);
}

int readGroup(char *fileName, char hosts[][32], char ids[][8]){
  FILE *fp= fopen(fileName, "r");
  if(fp==NULL)
	die("Can not open the group list file.");
  int i=0;
  while(!feof(fp)){
	fscanf(fp, "%s %s\n", hosts[i], ids[i]);
	i++;
  }
  fclose(fp);
  int j;
  printf("Group member list: \n");
  for(j=0; j<i; j++){
	printf("%d: N%s - %s\n", j+1, ids[j], hosts[j]);
  }
  return i;
}

void idToPort(char ids[][8], unsigned long ports[], int gsize){
	int i;
	char *end;
	int err=0;
	for(i=0; i<gsize; i++){
		ports[i]=strtoul(ids[i], &end, 10);
		if (ids[i] == end) {
			ports[i]=0;
			printf("Port conversion error on node %d. Can't convert %s to integer.\n", i+1, ids[i]);
			err++;
		}
	}
	if(err)
		die("Exit due to port conversion error.");
}
  
void checkGroup(unsigned long port, unsigned long ports[], int gsize){
  int i;
  bool in=false;
  for(i=0; i<gsize; i++){
	if(port==ports[i])
 	     in=true;	
  }
  if(!in)  
	dieF("Curent node is not in the group list.");
}

void PrintSocketAddress(const struct sockaddr *address, FILE *stream) {
  // Test for address and stream
  if (address == NULL || stream == NULL)
    return;

  void *numericAddress; // Pointer to binary address
  // Buffer to contain result (IPv6 sufficient to hold IPv4)
  char addrBuffer[INET6_ADDRSTRLEN];
  in_port_t port; // Port to print
  // Set pointer to address based on address family
  switch (address->sa_family) {
  case AF_INET:
    numericAddress = &((struct sockaddr_in *) address)->sin_addr;
    port = ntohs(((struct sockaddr_in *) address)->sin_port);
    break;
  case AF_INET6:
    numericAddress = &((struct sockaddr_in6 *) address)->sin6_addr;
    port = ntohs(((struct sockaddr_in6 *) address)->sin6_port);
    break;
  default:
    fputs("[unknown type]", stream);    // Unhandled type
    return;
  }
  // Convert binary to printable address
  if (inet_ntop(address->sa_family, numericAddress, addrBuffer,
      sizeof(addrBuffer)) == NULL)
    fputs("[invalid address]", stream); // Unable to convert
  else {
    fprintf(stream, "%s", addrBuffer);
    if (port != 0)                // Zero not valid in any socket addr
      fprintf(stream, "-%u", port);
  }
}
