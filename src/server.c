/*
 * RICCARDO GIOVANNI GUALIUMI
 *
 * Compilare con flag '-pthread'
*/

//headers
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <signal.h>
#include <pthread.h>

//defines and enumerations
#define MAXDIM 9000
#define MSGDIM 511
enum returnCode{OK, ERR};
enum type{START, SYNTAX, DATA, STATS};

//structs
struct socketThread{
    int simpleChildSocket;
    struct in_addr clientAddress;
    in_port_t clientPort;
};
typedef struct socketThread socketThread_t;

struct threadVariables{
    char message[MSGDIM];
    char msgIn[MAXDIM];
    char buffer[MAXDIM];
    socketThread_t client;
};
typedef struct threadVariables threadVariables_t;

//global variables
int simpleSocket = 0;
pthread_mutex_t mutex;  //to avoid race conditions of 'socketThread_t client' variable between threads.

//prototypes
int serverStart(char *port);    //returns 0 if an error has occurred
int acceptNewConnection(socketThread_t *client);      //returns 0 if an error has occurred
void sendMsg(enum returnCode Code, enum type msgType, char *msg, threadVariables_t *threadVar);    //send a message throught simpleChildSocket, to the client. If an error has occurred, it will displayed on console.
int receiveMsg(threadVariables_t *threadVar);
void errorCheck(int flag, threadVariables_t *threadVar);
int meanVariance(threadVariables_t *threadVar); //returns 4 if an error has occurred
char *string_trim_inplace(char *s);
void serverClose();
void signalServerClose(int sig);
void *serverThread(void *arg);

int main (int argc, char* argv[]){

    atexit(serverClose);

    //setting up sig handlers
    struct sigaction act;
    pthread_t thread;

    memset(&act, 0, sizeof(act));
    act.sa_handler = signalServerClose;

    sigaction(SIGINT,  &act, 0);
    sigaction(SIGTERM, &act, 0);

    //setting up mutex
    pthread_mutex_init(&mutex,NULL);

    if (argc!=2)    //too few arguments
    {
        fprintf(stderr, "Too few arguments! Please use the program as such: %s <port>\n", argv[0]);
        exit(1);    //argument/programming error code
    }
    if (!serverStart(argv[1]))
        exit(1);    //something went wrong when starting the server!

    while(1)
    {
        socketThread_t client;
        if (!acceptNewConnection(&client))
                exit(1);    //something went wrong when initializing new connection
        else
            pthread_create(&thread, NULL, serverThread, &client);
    }
}

int serverStart(char *port){
    int simplePort = 0;
    int returnStatus = 0;
    struct sockaddr_in simpleServer;

    printf("\nStarting server...\n");
    simpleSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (simpleSocket == -1) {

        fprintf(stderr, "Could not create a socket!\n");
        return 0;

    }
    else
        fprintf(stderr, "Socket created!\n");

    /* retrieve the port number where the server will listen */
    simplePort = atoi(port);

    /* setup the address structure */
    /* use INADDR_ANY to bind to all local addresses  */
    memset(&simpleServer, '\0', sizeof(simpleServer));
    simpleServer.sin_family = AF_INET;
    simpleServer.sin_addr.s_addr = htonl(INADDR_ANY);
    simpleServer.sin_port = htons((uint16_t)simplePort);

    /*  bind to the address and port with our socket  */
    returnStatus = bind(simpleSocket,(struct sockaddr *)&simpleServer,sizeof(simpleServer));

    if (returnStatus == 0) {
	    fprintf(stderr, "Bind completed!\n\n");
    }
    else {
        fprintf(stderr, "Could not bind to address!\n\n");
        close(simpleSocket);
        return 0;
    }

    /* lets listen on the socket for connections      */
    returnStatus = listen(simpleSocket, 5);

    if (returnStatus == -1) {
        fprintf(stderr, "Cannot listen on socket!\n");
        close(simpleSocket);
        return 0;
    }
    return 1;
}

int acceptNewConnection(socketThread_t *client){
    struct sockaddr_in clientName = { 0 };
    int clientNameLength = sizeof(clientName);
    int simpleChildSocket = 0;

    simpleChildSocket = accept(simpleSocket,(struct sockaddr *)&clientName, (socklen_t * restrict)&clientNameLength);

    if (simpleChildSocket == -1) {

        fprintf(stderr, "Cannot accept connections!\n");
        close(simpleSocket);
        return 0;

    }
    pthread_mutex_lock(&mutex); //entering in critical zone
    client->clientAddress = clientName.sin_addr;
    client->simpleChildSocket = simpleChildSocket;
    client->clientPort = clientName.sin_port;
    pthread_mutex_unlock(&mutex);   //quitting critical zone
    printf("Client %s:%d connected!\n\n", inet_ntoa(clientName.sin_addr), clientName.sin_port);
    return 1;
}

void sendMsg(enum returnCode Code, enum type msgType, char *msg, threadVariables_t *threadVar){
    char strCode[MSGDIM];
    char strType[MSGDIM];
    char strMsg[MSGDIM];


    strcpy(strMsg, " ");
    strcat(strMsg, msg);

    //choose the right Code
    if (Code == OK) strcpy(strCode, "OK");
    if (Code == ERR) strcpy(strCode, "ERR");


    //choose the right Type
    if (msgType == START)
    {
        char port[100];
        strcpy(strType, " START");
        strcat(strMsg, " ");
        strcat(strMsg, inet_ntoa(threadVar->client.clientAddress));
        strcat(strMsg, ":");
        sprintf(port, "%d", threadVar->client.clientPort);
        strcat(strMsg, port);
        strcat(strMsg, "!");
    }
    if (msgType == SYNTAX)  strcpy(strType, " SYNTAX");
    if (msgType == DATA)  strcpy(strType, " DATA");
    if (msgType == STATS)   strcpy(strType, " STATS");

    char *out = strcat(strCode, strcat(strType, strMsg));
    if (strlen(out)<=MSGDIM+1)
    {
        out[strlen(out)+1]='\0';
        out[strlen(out)]='\n';
        write(threadVar->client.simpleChildSocket, out, strlen(out));    //send message to client
    }
    else
        fprintf(stderr, "Exceeded the max lenght of 512 chars! Cannot send message to client!\n");    //lenght is too high. Error!
}

int receiveMsg(threadVariables_t *threadVar){
    memset(threadVar->buffer, '\0', MAXDIM);
    memset(threadVar->msgIn, '\0', MSGDIM);
    memset(threadVar->message, '\n', MSGDIM);
    ssize_t bytes;
    int declaredaredCnt;
    int cnt = 0;
    char msgInTok[MSGDIM];
    char *token = NULL;
    do
    {
        //reset variables for each iteration
        cnt = 0;
        declaredaredCnt = 0;

        //proceed to read from the client
        memset(threadVar->msgIn, '\0', MSGDIM); //cleaning the buffer
        bytes = read(threadVar->client.simpleChildSocket, threadVar->msgIn, sizeof(threadVar->msgIn));
        if (bytes>512)
        {
            strcpy(threadVar->message, "SYNTAX ERROR: received message exceeding 512 characters!");
            return 1;   //SYNTAX ERROR
        }
        printf("%s:%d > %s\n", inet_ntoa(threadVar->client.clientAddress), threadVar->client.clientPort, threadVar->msgIn);
        if (bytes==0)   //i've read 0 bytes, something went wrong: client disconnected'
            return 5;

        //syntax errors
        if (threadVar->msgIn[bytes-1]!='\n')
        {
            strcpy(threadVar->message, "SYNTAX ERROR: message not ending with newline!");
            return 1;  //SYNTAX ERROR: message doesn't end with '\n'! Protocol violated.
        }
        if (strlen(threadVar->msgIn)==0)
        {
            strcpy(threadVar->message, "Received an empty message!");
            return 1;  //empty message error
        }
        //checking for negative numbers/NAN
        token = strtok(strcpy(msgInTok, threadVar->msgIn), " ");
        while (token!=NULL)
        {
            for (size_t i=0; i<strlen(token)-1; i++)
            {
                if (!isdigit(token[i]))
                {
                    string_trim_inplace(token);
                    sprintf(threadVar->message, "Inserted data '%s' is not a number or it's negative!", token);
                    return 1;   //SYNTAX ERROR: inserted data is negative or Nan
                }
            }
            token = strtok(NULL, " ");
        }

        //checking for too many spaces
        for (ssize_t i=0; i<bytes; i++)
            if (threadVar->msgIn[i]==' ' && threadVar->msgIn[i+1]==' ')
            {
                strcpy(threadVar->message, "Sent message with too many spaces!");
                return 1;
            }

        //semantic errors
        //checking if the declared number of data is correct
        memset(msgInTok, '\0', sizeof(msgInTok));
        token = strtok(strcpy(msgInTok, threadVar->msgIn), " ");
        declaredaredCnt = atoi(token);

        if (declaredaredCnt>0)
        {
            while (token!=NULL)
            {
                cnt++;
                token = strtok(NULL, " ");
            }
            if (cnt-1 != declaredaredCnt)
            {
                strcpy(threadVar->message, "declared number of data doesn't match the actual number of data!");
                return 3;  //declared number of data doesn't match the actual number of data'
            }
        }

        //quit the iteration
        if (declaredaredCnt == 0)
        {
            break;
        }

        //send OK DATA to client
        char cntString[MAXDIM];
        memset(cntString, '\0', sizeof(cntString));
        sprintf(cntString, "%d", cnt-1);
        sendMsg(OK, DATA, cntString, threadVar);

        //removing the trailing counter from the message, and copying the data to buffer.
        string_trim_inplace(threadVar->msgIn); //removing trailing whitespaces (\n as well)
        token = strtok(threadVar->msgIn, " ");
        cnt = 0;    //used for skipping the first token
        while (token != NULL) {
            if (cnt==1) token = strtok(NULL, " ");
            if (cnt!=0 && token!=NULL)
            {
                strcat(threadVar->buffer, token);
                token = strtok(NULL, " ");
                strcat(threadVar->buffer, " ");
            }
            cnt++;
        }

    } while(1);

    string_trim_inplace(threadVar->msgIn); //remove all eventual whitespaces

    return 0;  //the message was good. No error was thrown. Proceeding calculating mean and variance.
}

void errorCheck(int flag, threadVariables_t *threadVar){

    if (flag == 1)
    {
        sendMsg(ERR, SYNTAX, threadVar->message, threadVar);
        close(threadVar->client.simpleChildSocket);    //closing connection with client
    }
    if (flag == 3)
    {
        sendMsg(ERR, DATA, threadVar->message, threadVar);
        close(threadVar->client.simpleChildSocket);    //closing connection with client
    }
    if (flag == 4)
    {
        sendMsg(ERR, STATS, threadVar->message, threadVar);
        close(threadVar->client.simpleChildSocket);    //closing connection with client
    }
    if (flag == 5)
        close(threadVar->client.simpleChildSocket);
}

int meanVariance(threadVariables_t *threadVar)
{
    // Compute mean (average of elements)
    float sum = 0;
    int n = 0;
    char *token = NULL;
    float a[MAXDIM];
    char tempN[MAXDIM];
    char tempV[MAXDIM];
    char tempM[MAXDIM];

    token = strtok(threadVar->buffer, " ");
    while (token!=NULL)
    {
        if (token!=NULL)
        {
            a[n] = (float)atoi(token);
            n++;
        }
        token = strtok(NULL, " ");
    }

    //not enought data. Error!
    if (n<=1)
    {
        sprintf(threadVar->message, "Cannot calculate variance of %d data!", n);
        return 4;
    }

    //calculating mean
    for (int i = 0; i < n; i++)
        sum += a[i];

    float mean = (float)sum/(float)n;

    // Compute sum squared
    // differences with mean.
    float sqDiff = 0;
    for (int i = 0; i < n; i++)
        sqDiff += (a[i] - mean) * (a[i] - mean);
    float variance = sqDiff/(float)(n-1);

    char out[MSGDIM];
    memset(out, '\0', MSGDIM);
    sprintf(tempN, "%d", n);
    sprintf(tempV, "%f", variance);
    sprintf(tempM, "%f", mean);
    strcpy(out, tempN);
    strcat(out, " ");
    strcat(out, tempM);
    strcat(out, " ");
    strcat(out, tempV);

    sendMsg(OK, STATS, out, threadVar);

    return 0;   //everything went according to plans.
}

//private function that returns a pointer to the trimmed string
char *string_trim_inplace(char *s) {
  while (isspace((unsigned char) *s)) s++;
  if (*s) {
    char *p = s;
    while (*p) p++;
    while (isspace((unsigned char) *(--p)));
    p[1] = '\0';
  }

  // If desired, shift the trimmed string

  return s;
}

void serverClose(){
    printf("\nServer shutting down...\n");
    pthread_mutex_destroy(&mutex);
}

void signalServerClose(int sig){
    exit(sig);
}

void *serverThread(void *arg){

    threadVariables_t threadVar;
    pthread_mutex_lock(&mutex); //entering critical zone
    socketThread_t *clientInfo = (socketThread_t*)arg;
    threadVar.client = *clientInfo;
    pthread_mutex_unlock(&mutex);   //leaving critical zone

    //new connection has been established. Server will greet client.
    sendMsg(OK, START, "Hello,", &threadVar);

    //await for a response after greeting, while checking for errors.
    int flag = receiveMsg(&threadVar);
    errorCheck(flag, &threadVar);

    //calculating mean and variance
    memset(threadVar.message, '\0', sizeof(threadVar.message));
    flag = meanVariance(&threadVar);
    errorCheck(flag, &threadVar);

    //evrything went to plan. Closing connection with client
    close(threadVar.client.simpleChildSocket);
    printf("Client %s:%d disconnected\n\n", inet_ntoa(threadVar.client.clientAddress), threadVar.client.clientPort);
    pthread_exit(NULL);
}
