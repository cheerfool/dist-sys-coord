#include <stdbool.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>

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

int readGroup(char *fileName, char hosts[][64], unsigned long ports[]){
  FILE *fp= fopen(fileName, "r");
  if(fp==NULL)
	die("Can not open the group list file.");
  int i=0;
  while(!feof(fp)){
	fscanf(fp, "%s %lu\n", hosts[i], ports+i);
	i++;
  }
  fclose(fp);
  int j;
  printf("Group member list: \n");
  for(j=0; j<i; j++){
	printf("%d: N%lu - %s\n", j+1, ports[j], hosts[j]);
  }
  return i;
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
