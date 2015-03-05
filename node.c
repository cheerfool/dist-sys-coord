#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <string.h>

#include "msg.h"
#include "tools.h"

int timeLimit= 10;
unsigned long  sendFailureProbability;
unsigned long  timeoutValue;
int sock;
struct addrinfo addrCriteria; // Criteria for address
unsigned long  port;
const int MAXLENGTH=256;
int gsize;
int myIndex;
char hosts[MAX_NODES][32];
char ids[MAX_NODES][8];
unsigned long ports[9];
unsigned long coord;
int electCount= 1;
bool electing= false;
bool waitCoord= false;
bool master=false;
struct clock myVectorClock[MAX_NODES];
unsigned int *myClock;
FILE *logfp;

int callElection(unsigned int electId);
int initMsgProcess(char* buffer, struct sockaddr_storage fromAddr);
int msgProcess(char* buffer, struct sockaddr_storage fromAddr);
int sendMsg(char host[], char id[], msgType type);
char* msgTypeString(msgType type);
int replyMsg(struct sockaddr_storage fromAddr, msgType type);
int passMsg(struct sockaddr_storage fromAddr, msgType type, unsigned int electId);
void *checkStatus(unsigned long *threadArgs);
void updateClock(struct clock ownClock[], struct clock newClock[]);
void copyClock(struct clock newClock[], struct clock ownClock[]);
void logClock();

void declareCoord(){
	int i;
	printf("[Action]: Declare current node N%u to be the coordinator.\n", port);
	coord= port;
	master= true;
	for(i=0; i<gsize; i++){
		if(ports[i]<port)
			sendMsg(hosts[i], ids[i], COORD);
	}
	electing=false;

	(*myClock)++;
	fprintf(logfp, "Declare to be coordinator\n");
	logClock();
}

// Handler for SIGALRM
void CatchAlarm(int ignored) {
	printf("Time out. ");
	if(waitCoord){
		electing=false;
		waitCoord=false;
		callElection(electCount++);
	}else if(electing){
		declareCoord();
	}else if(coord>port){
		callElection(electCount++);
	}
}


int main(int argc, char ** argv) {
	srandom(510);
	//parameters
	char *         groupListFileName;
	char *         logFileName;
	unsigned long  AYATime;

	
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

	//read and store the group list file and check if current node is in the group list, if not, terminate the program 
	gsize= readGroup(groupListFileName, hosts, ids, MAX_NODES);
	idToPort(ids, ports, gsize);
	myIndex= checkGroup(port, ports, gsize);

	//open the file to write logs
	logfp= fopen(logFileName, "w+");
	if(logfp==NULL)
		die("Can not write to the log file.");

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

	if (err) {
		printf("%d conversion error%sencountered, program exiting.\n",
			err, err>1? "s were ": " was ");
		return -1;
	}

	if(sendFailureProbability<0 || sendFailureProbability>100)
		die("Send failure probability should be an integer between 0-100");
	electCount+= port*1000;

	//Initialize current vector clock
	int j;
	for(j=0; j<MAX_NODES; j++){
		myVectorClock[j].nodeId= (j<gsize)?ports[j]:0;
		myVectorClock[j].time= (ports[j]==port)?1:0;
	}
	myClock= &myVectorClock[myIndex].time;


	printf("Starting up Node %d\n", port);
	printf("N%d {\"N%d\" : %d }\n", port, port, (*myClock)++);
	fprintf(logfp, "Starting N%d\n", port);
	logClock();


	// Construct the server address structure
	struct addrinfo addrCriteria; // Criteria for address
	memset(&addrCriteria, 0, sizeof(addrCriteria)); // Zero out structure
	addrCriteria.ai_family = AF_UNSPEC; // Any address family
	addrCriteria.ai_flags = AI_PASSIVE; // Accept on any address/port
	addrCriteria.ai_socktype = SOCK_DGRAM; // Only datagram socket
	addrCriteria.ai_protocol = IPPROTO_UDP; // Only UDP socket

	struct addrinfo *servAddr; // List of server addresses
	int rtnVal = getaddrinfo(NULL, argv[1], &addrCriteria, &servAddr);
	if (rtnVal != 0)
		die("getaddrinfo() failed.");

	// Create socket for incoming connections
	sock = socket(servAddr->ai_family, servAddr->ai_socktype, servAddr->ai_protocol);
	if (sock < 0)
		die("socket() failed");
	// Bind to the local address
	if (bind(sock, servAddr->ai_addr, servAddr->ai_addrlen) < 0)
		die("bind() failed");
	// Free address list allocated by getaddrinfo()
	freeaddrinfo(servAddr);
	
	// Set signal handler for alarm signal
	struct sigaction handler; // Signal handler
	handler.sa_handler = CatchAlarm;
	if (sigfillset(&handler.sa_mask) < 0) // Block everything in handler
		die("sigfillset() failed");
	handler.sa_flags = 0;
	if (sigaction(SIGALRM, &handler, 0) < 0)
		die("sigaction() failed for SIGALRM");

	//Ready to receive
	struct sockaddr_storage clntAddr; // Client address
	socklen_t clntAddrLen = sizeof(clntAddr);// Set Length of client address structure
	int maxBufSize= sizeof(struct msg)+1;
	char* recvBuffer= (char*)malloc(maxBufSize);
	memset(recvBuffer, 0, sizeof(recvBuffer));

	//Set the alarm
	alarm(timeoutValue);
	//Start to receive msgs
	ssize_t numBytesRcvd = recvfrom(sock, recvBuffer, maxBufSize, 0, (struct sockaddr *) &clntAddr, &clntAddrLen);
	if (numBytesRcvd < 0){
		if(errno==EINTR){
			callElection(electCount++);
		}else{
			die("recvfrom() failed");
		}
	}else{
		initMsgProcess(recvBuffer, clntAddr);
	}

	// Create thread for periodical status checking
	pthread_t threadID;
	int returnValue = pthread_create(&threadID, NULL, checkStatus, &AYATime);
	if (returnValue != 0)
		die("pthread_create() failed");
	printf("** Thread %ld created for periodical status checking.\n", (long int) threadID);

	//keep listensing and receiving msgs
	while(electing || *myClock < timeLimit){
		numBytesRcvd = recvfrom(sock, recvBuffer, maxBufSize, 0, (struct sockaddr *) &clntAddr, &clntAddrLen);
		if (numBytesRcvd < 0){
			if(errno!=EINTR)
				die("recvfrom() failed");
		}else{
			msgProcess(recvBuffer, clntAddr);
		}
	}
	// If you want to produce a repeatable sequence of "random" numbers
	// replace the call time() with an integer.
	//srandom(time());

	fclose(logfp);
	return 0;

}

int callElection(unsigned int electId){
	if(electing){
		printf("There is already an election started. So this will not start a new one.\n");
		return 0;
	}
	printf("N%u call an election.\n", port);
	electing= true;
	alarm(timeoutValue);
	int i;
	for(i=0; i<gsize; i++){
		if(ports[i]>port)
			forwardMsg(hosts[i], ids[i], ELECT, electId);
	}
}

int sendMsg(char host[], char id[], msgType type){
	forwardMsg(host, id, type, electCount);
}

int forwardMsg(char host[], char id[], msgType type, unsigned int electId){
	(*myClock)++;
	fprintf(logfp, "Send %s to N%s\n", msgTypeString(type), id);
	logClock();
	
	int luck= random()%100;
	if(luck< sendFailureProbability){
		printf("send() failed. Random number %d < %d (send failure probability).\n", luck, sendFailureProbability);
		return -1;
	}

	struct msg Msg;
	Msg.msgID= type;
	Msg.electionID= electId;
	copyClock(Msg.vectorClock, myVectorClock);

	int maxBufSize= sizeof(struct msg)+1;
	char* sendBuffer= (char*)malloc(maxBufSize);	
	memcpy(sendBuffer, &Msg, sizeof(Msg));

	struct addrinfo *targetAddr; // List of target addresses
	int rstVal = getaddrinfo(host, id, &addrCriteria, &targetAddr);
	if (rstVal != 0)
		die("getaddrinfo() failed");
	ssize_t numBytesSent = sendto(sock, sendBuffer, maxBufSize, 0, targetAddr->ai_addr, targetAddr->ai_addrlen);
	if (numBytesSent < 0)
		die("sendto() failed)");
	else if(numBytesSent != maxBufSize)
		die("sendto() failed, sent unexpected number of bytes");
	printf("-- Send a msg to %s-%s (N%s). [Type]: %s.\n", host, id, id, msgTypeString(type));
}

int replyMsg(struct sockaddr_storage fromAddr, msgType type){
	passMsg(fromAddr, type, electCount);
}

int passMsg(struct sockaddr_storage fromAddr, msgType type, unsigned int electId){
	unsigned int logPort= getSocketPort((struct sockaddr *)&fromAddr);
	(*myClock)++;
	fprintf(logfp, "Send %s to N%u\n", msgTypeString(type), logPort);
	logClock();

	int luck= random()%100;
	if(luck< sendFailureProbability){
		printf("send() failed. Random number %d < %d (send failure probability).\n", luck, sendFailureProbability);
		return -1;
	}
	struct msg Msg;
	Msg.msgID= type;
	Msg.electionID= electId;
	copyClock(Msg.vectorClock, myVectorClock);

	int maxBufSize= sizeof(struct msg)+1;
	char* sendBuffer= (char*)malloc(maxBufSize);	
	memcpy(sendBuffer, &Msg, sizeof(Msg));

	ssize_t numBytesSent = sendto(sock, sendBuffer, maxBufSize, 0, (struct sockaddr *)&fromAddr, sizeof(fromAddr));
	if (numBytesSent < 0)
		die("sendto() failed)");
	else if(numBytesSent != maxBufSize)
		die("sendto() failed, sent unexpected number of bytes");
	printf("-- Send a msg to ");
	unsigned int fromPort= PrintSocketAddress((struct sockaddr *)&fromAddr, stdout);
	printf(" (N%u). [Type]: %s.\n", fromPort, msgTypeString(type));
}

char* msgTypeString(msgType type){
	char* typeString;
	if(type==ELECT)
		return "ELECT";
	else if(type==ANSWER)
		return "ANSWER";
	else if(type==COORD)
		return "COORD";
	else if(type==AYA)
		return "AYA";
	else if(type==IAA)
		return "IAA";
	else
		return "Unknown";
}

int initMsgProcess(char* buffer, struct sockaddr_storage fromAddr){
	struct msg recvMsg;
	memcpy(&recvMsg, buffer, sizeof(recvMsg));
	msgType type= recvMsg.msgID;

	printf("++ Receive a msg from ");
	unsigned int fromPort= PrintSocketAddress((struct sockaddr *)&fromAddr, stdout);

	updateClock(myVectorClock, recvMsg.vectorClock);
	fprintf(logfp, "Receive %s from N%u\n", msgTypeString(type), fromPort);
	logClock();

	bool inGroup= false;
	int i;
	for(i=0; i<gsize; i++){
		if(ports[i]==(unsigned long)fromPort){
			inGroup= true;
			break;
		}
	}

	if(inGroup){
		printf(" (N%u).\n   ", fromPort);
		if(type==ELECT){
			printf("[Type]: ELECT.\t[Action]: Answer and forward the election. \n");
			//answer and forward here
			//answer
			unsigned int electId= recvMsg.electionID;
			passMsg(fromAddr, ANSWER, electId);
			callElection(recvMsg.electionID);
			return 1;
		}else if(type==COORD){
			printf("[Type]: COORD.\t[Action]: Set the coordinator to N%u.\n", fromPort);
			alarm(0);
			coord= fromPort;
			master=(fromPort==port)?true:false;
			electing= false;
			waitCoord= false;
			return 2;
		}else{
			printf("[Type]: %s.\t[Action]: Drop.\n", msgTypeString(type));
		}
	}else{
		printf(" (not in the group list)\n   Action: Discard.\n");
	}
		
	//if receive a msg but not valid, wait for the next msg and keep counting time
	struct sockaddr_storage clntAddr;
	socklen_t clntAddrLen = sizeof(clntAddr);
	int maxBufSize= sizeof(struct msg)+1;
	char* recvBuffer= (char*)malloc(maxBufSize);
	memset(recvBuffer, 0, sizeof(recvBuffer));
	ssize_t numBytesRcvd = recvfrom(sock, recvBuffer, maxBufSize, 0, (struct sockaddr *) &clntAddr, &clntAddrLen);
	if (numBytesRcvd < 0){
		if(errno==EINTR){
			printf("Time out. Call an election.\n");
			callElection(electCount++);
		}else{
			die("Recvfrom() failed");
		}
	}else{
		initMsgProcess(recvBuffer, clntAddr);
	}
	free(recvBuffer);
	return -1;
}

int msgProcess(char* buffer, struct sockaddr_storage fromAddr){
	printf("++ Receive a msg from ");
	unsigned int fromPort= PrintSocketAddress((struct sockaddr *)&fromAddr, stdout);

	bool inGroup= false;
	int i;
	for(i=0; i<gsize; i++){
		if(ports[i]==(unsigned long)fromPort){
			inGroup= true;
			break;
		}
	}
	if(!inGroup){
		printf(" (not in the group list)\n   [Action]: Discard.\n");
		return -1;
	}
	printf(" (N%u).\n   ", fromPort);

	struct msg recvMsg;
	memcpy(&recvMsg, buffer, sizeof(recvMsg));
	msgType type= recvMsg.msgID;

	updateClock(myVectorClock, recvMsg.vectorClock);
	fprintf(logfp, "Receive %s from N%u\n", msgTypeString(type), fromPort);
	logClock();

	if(type==ELECT){
		printf("[Type]: ELECT.\t");
		if(electing){
			printf("[Action]: Answer but not forward. (Already forwarded an election)\n");
			passMsg(fromAddr, ANSWER, recvMsg.electionID);
		}else{
			printf("[Action]: Answer and forward the election. \n");
			passMsg(fromAddr, ANSWER, recvMsg.electionID);
			callElection(recvMsg.electionID);
		}
	}else if(type==ANSWER){
		printf("[Type]: ANSWER.\t");
		if(electing){
			printf("[Action]: Reset the alarm to start waiting for COORD.\n");
			alarm(timeoutValue*(MAX_NODES+1));
			waitCoord=true;
		}else{
			printf("[Action]: Discard.\n");
		}
	}else if(type==COORD){
		printf("[Type]: COORD.\t[Action]: Set the coordinator to N%u.\n", fromPort);
		alarm(0);
		coord= fromPort;
		master=(fromPort==port)?true:false;
		waitCoord= false;
		electing= false;
	}else if(type==AYA){
		printf("[Type]: AYA.\t[Action]: Answer IAA.\n");
		replyMsg(fromAddr, IAA);
	}else if(type==IAA){
		printf("[Type]: IAA.\t");
		if(!electing && fromPort==coord){
			printf("[Action]: Cancel the time out alarm clock.\n");
			alarm(0);
		}else{
			printf("[Action]: Discard.\n");
		}	
	}
	return 1;
}

void *checkStatus(unsigned long *threadArgs){
  // Guarantees that thread resources are deallocated upon return
  int period= (int) *threadArgs;
  while(*myClock < timeLimit+2){
	  if(!electing && coord>=port){
		  if(master){
			  printf("## Periodical master status checking: I am alive (as a coordinator).\n");
			  (*myClock)++;
			  fprintf(logfp, "Coordinator status self-checking\n");
			  logClock();
		  }else{
			  printf("## Periodical master status checking: AYA sent to N%u.\n", coord);
			  int i;
			  char *host;
			  char *id;
			  for(i=0; i<gsize; i++){
				  if(ports[i]==coord){
					  host= hosts[i];
					  id=ids[i];
					  break;
				  }
			  }
			  alarm(period);
			  sendMsg(host, id, AYA);
		  }
	  }
	  sleep(period);
  }
  return (NULL);
}

void logClock(){
	bool init= true;
	int i;
	for(i=0; i<gsize; i++){
		struct clock curClock= myVectorClock[i];
		if(curClock.time>0){
			if(init){
				fprintf(logfp, "N%d {\"N%d\":%d", port, curClock.nodeId, curClock.time);
				init= false;
			}else{
				fprintf(logfp, ", \"N%d\":%d", curClock.nodeId, curClock.time);
			}
		}
	}
	fprintf(logfp, "}\n");
}

void copyClock(struct clock newClock[], struct clock ownClock[]){
	int i;
	for(i=0; i<MAX_NODES; i++){
		newClock[i].nodeId= ownClock[i].nodeId;
		newClock[i].time= ownClock[i].time;
	}
}

void updateClock(struct clock ownClock[], struct clock newClock[]){
	int i;
	for(i=0; i<gsize; i++){
		if(newClock[i].nodeId==ownClock[i].nodeId){
			if(newClock[i].time> ownClock[i].time)
				ownClock[i].time= newClock[i].time;
		}
	}
}
	

