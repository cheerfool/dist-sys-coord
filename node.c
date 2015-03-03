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

const int MAXLENGTH=256;
int gsize;
char hosts[MAX_NODES][32];
char ids[MAX_NODES][8];
unsigned long ports[9];
int electCount= 2;
struct clock myClockVector[MAX_NODES];
//void usage(char * cmd) 
//void die(char *msg)
//void dieF(char *msg)
//int readGroup(char* fileName, char[][] hosts, unsigned long[] ports)
//void checkGroup(unsigned long port, unsigned long[] ports)

// Handler for SIGALRM
void CatchAlarm(int ignored) {
	printf("Time out captured.\n");
}


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

	gsize= readGroup(groupListFileName, hosts, ids);
	idToPort(ids, ports, gsize);
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

	int j;
	//Initialize current vector clock
	for(j=0; j<gsize; j++){
		myClockVector[j].nodeId=ports[j];
		if(ports[j]==port)
			myClockVector[j].time=myClock;
		else
			myClockVector[j].time=-1;
	}

	//printf("My clock size: %d\n",sizeof(myClockVector));

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
	int sock = socket(servAddr->ai_family, servAddr->ai_socktype, servAddr->ai_protocol);
	if (sock < 0)
		die("socket() failed");

	// Bind to the local address
	if (bind(sock, servAddr->ai_addr, servAddr->ai_addrlen) < 0)
		die("bind() failed");

	// Free address list allocated by getaddrinfo()
	freeaddrinfo(servAddr);

	//Ready to receive
	struct sockaddr_storage clntAddr; // Client address
	// Set Length of client address structure (in-out parameter)
	socklen_t clntAddrLen = sizeof(clntAddr);

	// Block until receive message from a client
	char buffer[MAXLENGTH]; // I/O buffer
	// Size of received message

	int maxBufSize= sizeof(struct msg)+1;
	char* recvBuffer= (char*)malloc(maxBufSize);
	memset(recvBuffer, 0, sizeof(recvBuffer));
	


	// Set signal handler for alarm signal
	struct sigaction handler; // Signal handler
	handler.sa_handler = CatchAlarm;
	if (sigfillset(&handler.sa_mask) < 0) // Block everything in handler
		die("sigfillset() failed");
	handler.sa_flags = 0;
	if (sigaction(SIGALRM, &handler, 0) < 0)
		die("sigaction() failed for SIGALRM");


	//set the alarm
	alarm(timeoutValue);
	ssize_t numBytesRcvd = recvfrom(sock, recvBuffer, maxBufSize, 0, (struct sockaddr *) &clntAddr, &clntAddrLen);
	if (numBytesRcvd < 0){
		if(errno==EINTR){
			printf("Time out. Call an election.\n");
			struct msg electMsg;
			electMsg.msgID= ANSWER;
			electMsg.electionID= electCount;	
//			uint32_t hostInt = port;
//			uint32_t netInt = htonl(hostInt);
			electMsg.vectorClock[0]= myClockVector[0];

			char* sendBuffer= (char*)malloc(maxBufSize);	
			memcpy(sendBuffer, &electMsg, sizeof(electMsg));
			int bufSize= strlen(sendBuffer);
			printf("Sent : msgID-%d, electID-%d, node-N%d, time-%d\n", electMsg.msgID, electMsg.electionID, electMsg.vectorClock[0].nodeId, electMsg.vectorClock[0].time);	

			int i;
			for(i=0; i<gsize; i++){
				if(ports[i]<=port)
					continue;
				struct addrinfo *targetAddr; // List of target addresses
				int rstVal = getaddrinfo(hosts[i], ids[i], &addrCriteria, &targetAddr);
				if (rstVal != 0)
					die("getaddrinfo() failed");
				ssize_t numBytesSent = sendto(sock, sendBuffer, maxBufSize, 0, targetAddr->ai_addr, targetAddr->ai_addrlen);
				if (numBytesSent < 0)
					die("sendto() failed)");
				else if(numBytesSent != maxBufSize)
					die("sendto() failed, sent unexpected number of bytes");
				printf("Msg sent to %s:%d\n", hosts[i], ports[i]);
			}
		}else{
			die("recvfrom() failed");
		}
	}else{
		printf("Received msg from ");
		PrintSocketAddress((struct sockaddr *) &clntAddr, stdout);
		struct msg recvMsg;
		memcpy(&recvMsg, recvBuffer, sizeof(recvMsg));
//		uint32_t netInt = recvMsg.vectorClock[0].nodeId;
//		uint32_t hostInt = ntohl(netInt);
		printf(" : msgID-%d, electID-%d, node-N%d, time-%d\n", recvMsg.msgID, recvMsg.electionID, recvMsg.vectorClock[0].nodeId, recvMsg.vectorClock[0].time);	
		free(recvBuffer);
	}
	alarm(0);


	// If you want to produce a repeatable sequence of "random" numbers
	// replace the call time() with an integer.
	//srandom(time());
	srandom(510);

	int i;
	for (i = 0; i < 0; i++) {
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
