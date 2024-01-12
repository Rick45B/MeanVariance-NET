# MeanVariance-NET
## Description
Small client-server program calculating mean and variance of a stream of data by communicating with a custom application protocol, developed as a college assignment at University of Eastern Piedmont. Coded with POSIX compatible functions.<br>

The custom protocol works by sending a stream of data containing numbers separated by a single space, and terminated with '\n'. The server will then reply to the client, sending the mean and variance of the stream of data...<br>
```
Id est: 20 19 34 81 92 2 \n
```
...to the client, that will then proceed to show the results. <br>
**BOTH CLIENT AND SERVER HAVE BEEN CODED FOR LINUX, SO IT'S NOT GUARANTEED TO COMPILE ON WINDOWS.**
## Installation and usage
After downloading the client and server source files, compile them, remembering to add the **-pthreads** flag for the server, as it is multithreaded.
As an example, compiling with gcc:<br>
* server.c:
```
gcc -Wall -Wextra -Wconversion -pthread -o server server.c
```
* client.c:
```
gcc -Wall -Wextra -Wconversion -o client client.c
```
After compiling all the source code, the server must be started first:
```
./server <port>
```
And replace <<port>port> with the port you want the server to listen to.<br><br>

After the server has been started up, the client can be started as well:
```
./client <serverIP> <serverPort>
```
where <<serverIP>serverIP> is the IP of the server, and <<serverPort>serverPort> is the port the server is listening to.<br><br>

After the client has been initialized, just follow the instructions on screen to send the stream of data to be processed to the server, which will reply with the processed data.

## Features
* Server and client communicating through a custom application protocol, using TCP.
* Multi-threaded server supporting more connections.
* Guaranteed interruption of TCP connection in case of client error or unexpected errors thanks to the use of atexit() and sigaction() functions.
* Basic terminal GUI's for both the client and the server.
