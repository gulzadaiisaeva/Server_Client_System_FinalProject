/**************************************************/
/***** SYSTEM PROGRAMMING FINAL PROJECT************/

/*** GULZADA IISAEVA 131044085 ********************/

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <ctype.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/shm.h>
#include <signal.h>

#define MAXCLIENTS 1000
#define MAX_PATH 256
#define Log  "Server.log"

#define PERM (IPC_CREAT | 0666)

typedef unsigned short u_port;

/*Thread fonksiyonuna gonerilecek parametreler*/
typedef struct paramOfStruct{
	int matrixSize[2];
	int *commSocket;
} paramOfStruct;

/*global degiskenler*/
pthread_t tid[MAXCLIENTS];
pthread_mutex_t lock;
int socketForClosing;
int countThread=0;
/*************************/

/*  socket acma
    socket - bind - listen kitaptan alindi*/
int u_open(u_port port);
/* accept*/
int u_accept(int fd, char *hostn, int hostnsize);
/* adress to hostname kitaptan alindi*/
void addr2name(struct in_addr addr, char *name, int namelen);

/*threade gonderilecek fonksiyonlar*/
void* funcToThread(void* structureParam);
void serverClientFunc(int matrixSize[2],int* commSocket);

/*shared memory kitaptan alindi*/
int detachandremove(int shmid, void *shmaddr);
/*sinyal*/
void signalHandler(int signo);
/*main function*/
void allDoThingServer(u_port port);


int main(int argc, char *argv[])
{

    u_port port;

    if(argc != 2){
      fprintf(stderr, "Usage: ./server portNo Ip\n");
      fprintf(stderr, "Example: ./server 53336 \n");
      return -1;
    }
	signal(SIGINT,signalHandler);
    port = (u_port)atoi(argv[1]);
    allDoThingServer(port);

    return 0;
}


void allDoThingServer(u_port port)
{
    int listenfd = 0, communicationfd = 0;
    int error,matrixSize[2];
    pid_t serverPid = getpid();
		 char client[MAX_PATH];
		int j;
    paramOfStruct parameters;

    if((listenfd = u_open(port)) == -1){
      perror("Failed to open socket !\n");
      return;
    }
    fprintf(stderr, "[%ld]: Waiting for connection on port %d\n",
    (long)serverPid, (int)port);

    while(1)
    {
			communicationfd = u_accept(listenfd, client, MAX_PATH);
			socketForClosing=communicationfd;
      fprintf(stderr, "[%ld]:connected to %s\n", (long)getpid(), client);
      read(communicationfd, matrixSize, 2*sizeof(int));
      parameters.matrixSize[0]=matrixSize[0];
      parameters.matrixSize[1]=matrixSize[1];
			parameters.commSocket=communicationfd;

      if( error == pthread_create( tid+countThread , NULL ,  funcToThread , &parameters))
      {
					if(error==-1){
					fprintf(stderr, "Failed to create thread: %s\n", strerror(error));;}
					tid[countThread] = pthread_self();
      }

			countThread++;
			fprintf(stderr, "Total number of the connected clients: %d\n", countThread);

    }

		for (j = 0; j < countThread; j++)
	  {
			if (pthread_equal(pthread_self(), tid[j]))
			continue;
			if (error = pthread_join(tid[j], NULL))
			if(error==-1){
			fprintf(stderr, "Failed to join thread: %s\n", strerror(error));}
    }

}

int u_open(u_port port){
  struct sockaddr_in server;
	int sock;
	int error;
	int true = 1;
	int flags;


	if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		return -1;

	if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&true, sizeof(true)) == -1){
		error = errno;
		while((close(sock) == -1) && (errno == EINTR));
		errno = error;
    return -1;
	}
	memset((char *)&server, '\0', sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons((short)port);

  if ((bind(sock, (struct sockaddr *)&server, sizeof(server)) == -1) ||
    (listen(sock, MAXCLIENTS) == -1)) {
    error = errno;
    while ((close(sock) == -1) && (errno == EINTR));
    errno = error;
    return -1;
  }


	return sock;
}

int u_accept(int fd, char *hostn, int hostnsize) {
    int len = sizeof(struct sockaddr);
    struct sockaddr_in netclient;
    int retval;

    while (((retval =accept(fd, (struct sockaddr *)(&netclient), &len)) == -1) &&(errno == EINTR)) ;
    if ((retval == -1) || (hostn == NULL) || (hostnsize <= 0))
    return retval;

    addr2name(netclient.sin_addr, hostn, hostnsize);

    return retval;
}

void addr2name(struct in_addr addr, char *name, int namelen) {
    struct hostent *hostptr;

    hostptr = gethostbyaddr((char *)&addr, 4, AF_INET);
    if (hostptr == NULL)
      strncpy(name, inet_ntoa(addr), namelen-1);
    else
      strncpy(name, hostptr->h_name, namelen-1);
    name[namelen-1] = 0;
}

void* funcToThread(void* structureParam)
{
  paramOfStruct* param= (paramOfStruct*) structureParam;
  serverClientFunc(param->matrixSize,param->commSocket);

}
void serverClientFunc(int matrixSize[2],int* commSocket)
{
		FILE *output;
		pid_t f1,f2;
		int row=matrixSize[0],col=matrixSize[1];
		int i,j,*random;
		int matrixAid,matrixBid;
		int *matrixA;
		int *matrixB;
		int fd[2],flag=0;

		if (pipe(fd) == -1)
		perror("Failed to create the pipe");
		write(fd[1],&flag,1);

		output=fopen(Log,"a");

		if(pthread_mutex_init(&lock,NULL)!=0)
		{
			perror("Failed to init mutex\n");
		}
		/*shared memory for matrixA*/
		if ((matrixAid = shmget(IPC_PRIVATE, row*col* sizeof(int), PERM)) == -1) {
				perror("Failed to create shared memory segment");
				return 1;
	 }
		if ((matrixA = (int *)shmat(matrixAid, NULL, 0)) == (void *)-1) {
			perror("Failed to attach shared memory segment");
			if (shmctl(matrixAid, IPC_RMID, NULL) == -1)
			perror("Failed to remove memory segment");
			return 1;
		}
		/*end of shared memory for matrix*/

		/*shared memory for matrixA*/
		if ((matrixBid = shmget(IPC_PRIVATE, row* sizeof(int), PERM)) == -1) {
				perror("Failed to create shared memory segment");
				return 1;
	 }
		if ((matrixB = (int *)shmat(matrixBid, NULL, 0)) == (void *)-1) {
			perror("Failed to attach shared memory segment");
			if (shmctl(matrixBid, IPC_RMID, NULL) == -1)
			perror("Failed to remove memory segment");
			return 1;
		}
		/*end of shared memory for matrix*/

		if((f1=fork())==-1)
		{
			perror("Failed to create a process");
		}

		if(f1==0)
		{
			/*Generating Matrix A*/
			srand(pthread_self());
			random=(int*)malloc(sizeof(int));
			for(i=0;i<col*row;i++)
			{
				random[0]=rand() % 10;
				memcpy(matrixA+i,random, 1);
			}
			free(random);
			/*End of Generating Matrix A*/

			/*Generating Matrix B*/
			srand(pthread_self());
			random=(int*)malloc(sizeof(int));
			for(i=0;i<row;i++)
			{
				random[0]=rand() % 10;
				memcpy(matrixB+i,random, 1);
			}
			free(random);
			/*End of Generating Matrix B*/

			/*printing Matrix A to Log file*/
			fprintf(output, "A = [ " );
			for (i=0;i < row*col;i++) {
				fprintf(output,"%d ", matrixA[i] );
				if (i%row==row-1) {
				fprintf(output,"; ");
				}
			}
			fprintf(output," ]\n");
			/*end of printing Matrix A to Log file*/

			/*printing Matrix B to Log file*/
			fprintf(output, "B = [ " );
			for (i=0;i < row ;i++) {
				fprintf(output,"%d ", matrixB[i] );
			}
			fprintf(output,"] \n");
			/*end of printing Matrix B to Log file*/

			pthread_mutex_lock(&lock);
			write(commSocket, matrixA,  row*col*sizeof(int));
			pthread_mutex_unlock(&lock);
			pthread_mutex_lock(&lock);
			write(commSocket, matrixB,  row*sizeof(int));
			pthread_mutex_unlock(&lock);

			exit(0);
		}

		if((f2=fork())==-1)
		{
			perror("Failed to create a process");
		}

		if(f2==0)
		{


			exit(0);
		}

		while(wait(NULL)>0);
		fclose(output);
		pthread_mutex_destroy(&lock);
		detachandremove(matrixAid, matrixA) ;
		detachandremove(matrixBid, matrixB) ;
}


int detachandremove(int shmid, void *shmaddr) {
	int error = 0;
	if (shmdt(shmaddr) == -1)
	error = errno;
	if ((shmctl(shmid, IPC_RMID, NULL) == -1) && !error)
	error = errno;
	if (!error)
	return 0;
	errno = error;
	return -1;
}

void signalHandler(int signo){
 	int i;
	time_t t;
  time(&t);
	printf("CTRL + C signal has caught %s\n",ctime(&t) );
	for (i = 0; i < countThread; i++) {
		pthread_kill(tid[i],SIGINT);
	}
	close(socketForClosing);

	exit(signo);
}
