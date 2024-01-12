/*
 * RICCARDO GIOVANNI GUALIUMI
*/

//headers
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <ctype.h>

//defines
#define MAXDIM 9000
#define MSGDIM 511



//global variables
int simpleSocket = 0;
int simplePort = 0;
int returnStatus = 0;
struct sockaddr_in simpleServer;
char buffer[513];
char code[MSGDIM];
char type[MSGDIM];

//prototypes
int clientStart(char *address, char *port);
char *readFromServer(char *code, char *type);
int mainMenu();
void askData();
char *string_trim_inplace(char *s);
void closingClient();
void signalClosingClient(int sig);

int main(int argc, char *argv[]) {

    printf("\nStarting up client...\n");
    atexit(closingClient);

    //setting up sig handlers (To ensure that the connection will close even if user press Ctrl-C)
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    act.sa_handler = signalClosingClient;

    sigaction(SIGINT,  &act, 0);
    sigaction(SIGTERM, &act, 0);
    char *serverResponse;   //for server's messages

    if (3 != argc)
    {
        fprintf(stderr, "Too few arguments! Please use the program as such: %s <server-ip> <port>\n", argv[0]);
        exit(1);    //argument/programming error code.
    }
    if (!clientStart(argv[1], argv[2]))
        exit(1);

    //greeting message from the server, defined by the protocol.
    fflush(NULL);
    serverResponse = readFromServer(code, type);
    if (strcmp(code, "OK")==0 && strcmp(type, "START")==0)
        printf("Server > %s\n", serverResponse);
    else    //client hasn't been greeted properly. Might be connected with malignant host. Aborting connection...
    {
        printf("Server hasn't replied with a proper greeting message. Are you sure the ip address and port are correct?\n");
        exit(3);    //special error code: bad greeting from server
    }

    if (!mainMenu()) //client wants to exit and close connection to the server
        exit(0);
    else
    {
        askData();
        //after sending data, i should always listen for a response
        printf("Awaiting for server response...\n");
        serverResponse = readFromServer(code, type);

        //unlawful message. Protocol was violated. Connection must be terminated
        if ((strcmp(type, "DATA")!=0 && strcmp(type, "STATS")!=0) || (strcmp(code, "ERR")!=0 && strcmp(code, "OK")!=0))
        {
            fprintf(stderr, "Server replied with an unlawful message! Perhaps it got corrupted?\n");
            exit(2);    //protocol error code
        }

        //Received an error code from the server. Connetion must be aborted.
        if (strcmp(code, "ERR")==0 && (strcmp(type, "SYNTAX")==0 || strcmp(type, "STATS")==0))
        {
            printf("Server > %s", serverResponse);
            exit(2);    //protocol error code.
        }
        else    //server replied with good mean and variance. Showing the results to user... -> i've Received 'OK STATS <number> <mean> <variance>'
        {
            int n = 0;
            int dataQTY=-1;
            float mean=-1;
            float variance=-1;
            char *token = NULL;

            token = strtok(buffer, " ");
            while (token!=NULL)
            {
                if (token!=NULL && n==0) dataQTY = atoi(token);
                if (token!=NULL && n==1) mean = (float)atof(token);

                if (token!=NULL && n==2) variance = (float)atof(token);
                n++;
                token = strtok(NULL, " ");
            }
            printf("Server replied with good data!\n");
            printf("Data uploaded: %d\n", dataQTY);
            printf("Mean: %f\n", mean);
            printf("Variance: %f\n", variance);
            exit(0);
        }
    }


    close(simpleSocket);
    exit(0);

}

int clientStart(char *address, char *port){

    simpleSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (simpleSocket == -1) {

        fprintf(stderr, "Could not create a socket!\n");
        return 0;

    }
    else {
	    fprintf(stderr, "Socket created!\n");
    }

    /* retrieve the port number for connecting */
    simplePort = atoi(port);

    memset(&simpleServer, '\0', sizeof(simpleServer));
    simpleServer.sin_family = AF_INET;
    simpleServer.sin_addr.s_addr=inet_addr(address);
    simpleServer.sin_port = htons((uint16_t)simplePort);

    printf("Connecting to %s, port %s\n", address, port);
    returnStatus = connect(simpleSocket, (struct sockaddr *)&simpleServer, sizeof(simpleServer));

    if (returnStatus == 0) {
        fprintf(stderr, "Connect successful!\n");
    }
    else
    {
        fprintf(stderr, "Could not connect to address!\n");
        close(simpleSocket);
        return 0;
    }
    return 1;
}

char *readFromServer(char *code, char *type){
    size_t beginIndex = 0;
    size_t endIndex = 0;
    char *token = NULL;
    memset(buffer, '\0', sizeof(buffer));   //cleaning buffer
    memset(code, '\0', MSGDIM);   //cleaning code buffer
    memset(type, '\0', MSGDIM);   //cleaning type buffer
    ssize_t bytes = read(simpleSocket, buffer, sizeof(buffer));
    if (bytes>512)
    {
        printf("Server message exceeded the maximum lenght of 512 characters!\n");
        exit(2);    //protocol error code
    }
    endIndex = strlen(buffer)+1;
    token = strtok(buffer, " ");
    beginIndex += strlen(token)+1;

    //get the code and the type of message
    strncpy(code, buffer, beginIndex);
    token = strtok(NULL, " ");
    strncpy(type, buffer+beginIndex, strlen(token));
    beginIndex += strlen(token)+1;

    /*return strncpy(buffer, buffer+beginIndex, endIndex-beginIndex);*/
    memmove(buffer, buffer+beginIndex, endIndex-beginIndex);
    return buffer;
}

int mainMenu(){
    int selection = 1; //defaults to 1.
    printf("This program has the aim to calculate mean and variance of a series of number provided by the client to the server.\n");
    printf("Press 1 to enter the data for which to calculate the mean and variance, 0 to close the program and disconnect from the server...\n");
    do
    {
        printf("\n> ");
        scanf("%d", &selection);
        getchar();
        if (selection!=1 && selection!=0)
            printf("Please digit a valid option!\n");
    }
    while (selection!=1 && selection!=0);
    return selection;
}

void askData(){
    char outTemp[MAXDIM];
    int cnt = 0;
    int flag = 1; //initially set to true
    char *token = NULL;
    char outTempTok[MAXDIM];

    do
    {
        cnt = 0;
        flag = 1; //resets flag at the beginning of the iteration.
        printf("\nInsert all the data to send to server for which to calculate the mean and variance, using the following format: <data1> <data2> <dataN>");
        do
        {
            printf("\n> ");
            fgets(outTemp, MAXDIM, stdin);  //puts '\n' at the end of the characters sequence by default!
        } while (strlen(outTemp)==1);

        //checking if the user inserted a whitespace as last character
        string_trim_inplace(outTemp);

        printf("\n"); //for a better GUI

        //checking if the data is valid, while counting the number of datas.
        token = strtok(strcpy(outTempTok, outTemp), " ");
        while (token!=NULL)
        {
            cnt++;
            if (token==NULL)    printf("Il token è NULL!\n");
            for (size_t i=0; i<strlen(token)-1; i++)
            {
                if (!isdigit(token[i]))
                {
                    fprintf(stderr, "Inserted data at index %d is not a number or it's negative! It values, in fact: %s\n", cnt, token);
                    flag = 0;
                    break;
                }
            }
            token = strtok(NULL, " ");
        }
        for (size_t i = 0; i<strlen(outTemp); i++)
        {
            if (i==0 && outTemp[i]==' ')
            {
                fprintf(stderr, "Inserted a space at the head of the data list!\n");
                flag = 0;
                break;
            }
            if (outTemp[i]==' ' && outTemp[i+1]==' ')
            {
                fprintf(stderr, "Inserted too many spaces!\n");
                flag = 0;
                break;
            }
        }
        printf("\n");
        if (flag == 0) continue; //if an error has occurred already, skip the next part

        //constructing the message
        char out[MAXDIM];
        char cntString[MAXDIM];
        memset(out, '\0', sizeof(out));
        memset(cntString, '\0', sizeof(cntString));
        sprintf(cntString, "%d ", cnt);
        strcpy(out, cntString);
        strcat(out, outTemp);

        if (strlen(out)>=512)
        {
            fprintf(stderr, "Proposed message exceeds 512 characters! Try inputting less datas\n");
            flag = 0;
            continue;
        }
        else    //everything is correct. I can proceed to send the message
        {
            printf("Sending data...\n");
            out[strlen(out)] = '\n';    //capping string with last character '\n'
            write(simpleSocket, out, strlen(out));  //sending string up to '\n' (included), while all the other '\0' are excluded.
            printf("Uploading complete!\n");
            char *num = readFromServer(code, type);
            if ((strcmp(type, "DATA")!=0 && strcmp(type, "SYNTAX")!=0) || (strcmp(code, "ERR")!=0 && strcmp(code, "OK")!=0))   //unlawful answer
            {
                fprintf(stderr, "Server replied with an unlawful message! Perhaps it got corrupted?\n");
                exit(2);    //protocol error code
            }
            if (strcmp(code, "ERR")==0)
            {
                fprintf(stderr, "Server > %s\n", num);
                exit(2);    //protocol error code.
            }
            num = string_trim_inplace(num);
            printf("Server > Received %s datas\n", num);

            //checking if the number of data from the server's message match the transmitted ones.
            if (atoi(num) != cnt)
            {
                fprintf(stderr, "Number of confirmed data from the server doesn't match the transmitted ones! Results might be corrupted!\n");
                break;
            }

            printf("\nDo you want to insert more data? Y/N > ");
            char answer;
            do
            {
                scanf("%c", &answer);
                getchar();
                if (answer!='Y' && answer!='N' && answer!='y' && answer!='n')
                {
                    fprintf(stderr, "Please insert a valid input!\n");
                    printf("\n> ");
                }
            }
            while (answer!='Y' && answer!='N' && answer!='y' && answer!='n');

            if (answer=='N' || answer=='n')
                flag = 1;
            else
                flag = 0;   //this will keep me from quitting from the iteration.
        }
    }
    while (flag==0);  //if flag == 0, then something went wrong when inserting data, or i'm not done yet sending data.

    //send 0 to signal to the server that all the data has been sent
    write(simpleSocket, "0\n", strlen("0\n"));
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

void closingClient(){
    printf("\nDisconnecting from server...\n");
    close(simpleSocket);
    printf("Shutting down client...\n");

}

void signalClosingClient(int sig){
    exit(sig);
}
