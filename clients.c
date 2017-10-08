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
#include <semaphore.h>
#include <signal.h>

#define Log  "Clients.log"
#define MAXCLIENTS 1000

typedef unsigned short u_port;

/*Thread fonksiyonuna gonerilecek parametreler*/
typedef struct paramOfStruct{
	u_port port;
	char* ipAddr;
	int matrixSize[2];
} paramOfStruct;

/*****global degiskenler*****/
sem_t semlock;
double totalmsecs = 0;
double arrayForStd[1024];
int count=0;
pthread_t tid[MAXCLIENTS];
int ClientsNum=0;
int socketForClosing;
/**end of global*/

/*connect*/
int u_ignore_sigpipe(void);
int u_connect(u_port port, char *hostn);
/*************************/

/*threade gonderilecek fonksiyonlar*/
void* funcToThread(void* structureParam);
void clientServerFunc(u_port port, char* ipAddr,int matrixSize[2]);
/****************************************************************/

/*Standart sapma hesaplama*/
double calculateSD(double data[], double mean ,double sum, int size );

/*sinyal */
void signalHandler(int signo);

/*ana fonksiyon*/
void allDoThingClients(u_port port, char* ipAddr,int matrixM,int matrixP,int numOfClients);


int main(int argc, char *argv[])
{
    u_port port;
    char ipAddr[16] = "\0";
    int matrixM,matrixP,numOfClients;

    if(argc != 6){
      fprintf(stderr, "Usage: ./clients m p noOfClients portNo Ip\n");
      fprintf(stderr, "Example: ./clients 3 2 5 53336 127.0.0.1\n");
      return -1;
    }
		signal(SIGINT,signalHandler);
		matrixM=atoi(argv[1]);
    matrixP=atoi(argv[2]);
    numOfClients=atoi(argv[3]);
    port = (u_port)atoi(argv[4]);
    strcpy(ipAddr, argv[5]);
		ClientsNum=numOfClients;

    if (sem_init(&semlock, 0, 1) == -1) {
      perror("Failed to initialize semaphore");
      return 1;
    }
    allDoThingClients(port,ipAddr,matrixM,matrixP,numOfClients);


    return 0;
}

/*girilen clinet sayisi kadar thread olusturuyor*/
void allDoThingClients(u_port port, char* ipAddr,int matrixM,int matrixP,int numOfClients)
{
	 FILE *output;

   paramOfStruct parameters;
   int error,i,j;
	 double std;
	 output=fopen(Log,"a");

   parameters.port=port;
   parameters.ipAddr=ipAddr;
   parameters.matrixSize[0]=matrixP;
   parameters.matrixSize[1]=matrixM;

   for (i = 0; i < numOfClients; i++)
   {
      if (error = pthread_create(tid + i, NULL, funcToThread, &parameters)) {
 			if(error==-1){
			fprintf(stderr, "Failed to create thread: %s\n", strerror(error));}
      tid[i] = pthread_self();
      }
   }

   for (j = 0; j < numOfClients; j++)
   {
     if (pthread_equal(pthread_self(), tid[j]))
     continue;
     if (error = pthread_join(tid[j], NULL))
		 if(error==-1){
     fprintf(stderr, "Failed to join thread: %s\n", strerror(error));}

   }
	 printf("[%d] Average connection time %lf ms\n", (int)getpid(),totalmsecs/numOfClients);
	 fprintf(output,"[%d] Average connection time %lf ms\n", (int)getpid(),totalmsecs/numOfClients);
	 std=calculateSD(arrayForStd,totalmsecs/numOfClients ,totalmsecs, numOfClients );
	 printf("[%d] Standard Deviation  %lf ms\n\n",(int)getpid(), std);
	 fprintf(output,"[%d] Standard Deviation  %lf ms\n\n",(int)getpid(), std);
	
	 close(output);
}
/*kitaptan alindi*/
int u_connect(u_port port, char *hostn) {
    struct sockaddr_in server_addr;
    struct hostent *hp;
    struct in_addr ipv4addr;
    int socketFD,error;

    /* Creating communication endpoint (socketFD) */
    if((socketFD = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    return -1;


    memset((char *)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((short)port);

    // Converts character string into network address
    inet_pton(AF_INET, hostn, &ipv4addr);
    // Get host name
    hp = gethostbyaddr(&ipv4addr, sizeof(ipv4addr), AF_INET);

    if(hp == NULL)
    {
      error = errno;
      while((close(socketFD) == -1) && (errno == EINTR));
      errno = error;
      return -1;
    }
    else{
      strncpy((char *)hp->h_addr, (char *)&server_addr.sin_addr.s_addr, hp->h_length);
    }

    /* Request connection from the server */
    if((connect(socketFD, (struct sockaddr *)&server_addr, sizeof(server_addr))) == -1){
      error = errno;
      while((close(socketFD) == -1) && (errno == EINTR));
      errno = error;
      return -1;
    }

    return socketFD;
}

void* funcToThread(void* structureParam)
{
  paramOfStruct* param= (paramOfStruct*) structureParam;
  clientServerFunc(param->port, param->ipAddr, param->matrixSize);

}
void clientServerFunc(u_port port, char* ipAddr,int matrixSize[2])
{
	  FILE *output;
    int communicationfd=0;
		int row=matrixSize[0],col=matrixSize[1];
    int error,i,j;
		int temp;
		struct timeval start, stop;
		double secs;
		int matrixA[row*col],matrixB[row];
		output=fopen(Log,"a");

		gettimeofday(&start, NULL);
    /****************** entry section *******************************/
    while (sem_wait(&semlock) == -1)
    {
        if(errno != EINTR) {
        fprintf(stderr, "Thread failed to lock semaphore\n");
        return NULL;
        }
    }
    /****************** start of critical section *******************/
    if((communicationfd = u_connect(port,ipAddr)) == -1){
     perror("Failed to connect to the server!\n");
     return;
    }
    fprintf(stderr, "[%lu]Connected to the server[%s] with port[%d]\n",pthread_self(), ipAddr, (int)port);

   
    /*fprintf(stderr, "Sendind data to server\n");*/
    write(communicationfd, matrixSize,  2*sizeof(int));
		read(communicationfd, matrixA, row*col*sizeof(int));
		read(communicationfd, matrixB, row*sizeof(int));

		/*printing Matrix A to Log file*/
		fprintf(output, "[%lu] \nA = [ " ,pthread_self());
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

			gettimeofday(&stop, NULL);
			secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
			totalmsecs+=secs;
			arrayForStd[count]=secs;
			count++;

       /****************** exit section ********************************/

    if (sem_post(&semlock) == -1)
    {
      fprintf(stderr, "Thread failed to unlock semaphore\n");
      return NULL;
    }
		close(output);
}
double calculateSD(double data[], double mean ,double sum, int size )
{
    double standardDeviation = 0.0;

    int i;

    for(i=0; i<size; ++i)
        standardDeviation += pow(data[i] - mean, 2);

    return sqrt(standardDeviation/10);
}
void signalHandler(int signo){
	FILE *output;
	time_t t;
  time(&t);
	int i=0;
	output=fopen(Log,"a");
	fprintf(output, "CTRL + C signal has caught %s\n",ctime(&t) );
	printf("CTRL + C signal has caught %s\n",ctime(&t) );
	close(output);
	for (i = 0; i < count; i++) {
		pthread_kill(tid[i],SIGINT);
	}
	close(socketForClosing);
	exit(signo);
}
