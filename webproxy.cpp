/* File Name: webproxy.cpp
*  Author: Jaron Baldazo
*  Compile: g++ webproxy.cpp -o webproxy
*  Run: ./webproxy
*
*  References:
*  https://www.binarytides.com/socket-programming-c-linux-tutorial/         //Basics of Socket Programmin
*  https://pages.cpsc.ucalgary.ca/~carey/CPSC441/examples/testserver.c      //Carey Test Server
*  https://www.jmarshall.com/easy/http/                                     //HTTP references
*  https://man7.org/linux/man-pages/man2/                                   //Documentation for linux functions
*  https://linux.die.net/man/3/setsockopt                                   //Documentation for setsockopt 
*  https://d2l.ucalgary.ca/d2l/le/content/401094/viewContent/4896480/View   //HTTP Tutorial Powerpoint
*  https://d2l.ucalgary.ca/d2l/le/content/401094/viewContent/4885824/View   //Socket Programming Tutorial Powerpoint
*  https://stackoverflow.com/questions/4181784/how-to-set-socket-timeout-in-c-when-making-multiple-connections   //Socket timeouts   
*/

#include <stdio.h>      //print function; puts()
#include <string.h>     //parsing requests     
#include <sys/socket.h> //socket
#include <arpa/inet.h>  //inet_addr
#include <unistd.h>     //close
#include <netdb.h>      //getting host name
#include <time.h>       //timer for telnet socket

#define PORTMAIN 22334             //port to connect to from browser
#define PORTTELNET 43322           //port to connnect to from telnet
#define MESSAGELENGTH 2000000      //max message length for char[]
#define BLOCKMESSAGELENGTH 1000    //max message length for telnet requests and block list word size

/* TODO/NOTES:
*  136.159.2.17 -> ip pages.cpsc.ucalgary.ca
*  GET /~carey/CPSC441/ass1/error.html HTTP/1.1 -> request for error page
*  Host: pages.cpsc.ucalgary.ca        
*/

int main (int argc, char *argv[]){
    int parentSocketDesc, childSocketDesc;           //file descriptor for server side proxy socket for communicating with web browser and client side proxy
    int clientProxyDesc;                             //file descriptor for client side proxy socket for communicating with web server
    int telnetParentDesc, telnetChildDesc;           //file descriptors for telnet server sockets for communicating with user
    int c;                                           //size of structure sockaddr_in
    struct sockaddr_in server, client;               //structure sockaddr_in for server side proxy
    struct sockaddr_in clientProxy;                  //structure sockaddr_in for client side proxy
    struct timeval timer;                            //structure timeval for telnet socket timer
    char browserRequest[MESSAGELENGTH];              //request sent from web browser
    int breqSize;                                    //browser request size in bytes
    char *hostName;                                  //host name for connecting to web server
    struct hostent* address;                         //address of web server; used when getting host name
    int sreqSize;                                    //request being sent size in bytes
    char webServerReply[MESSAGELENGTH];              //reply from web server
    int srepSize;                                    //reply being sent from web server in bytes;
    int blockCounter = 0;                            //count how many words are in the block list
    char blockList[5][BLOCKMESSAGELENGTH];           //2d char array for block list; maximum of 5 words to be blocked with max length BLOCKMESSAGE LENGTH
    //error request being sent to web server when url needs to be blocked
    char errorRequest [] = "GET http://pages.cpsc.ucalgary.ca/~carey/CPSC441/ass1/error.html HTTP/1.1\r\nHost: pages.cpsc.ucalgary.ca\r\nContent-Type: text-plain\r\n\r\n";
    char telnetRequest[MESSAGELENGTH];               //telnet request for block/unblock commands
    int treqSize;                                    //telnet request size in bytes
    //html code for error page
    char errorPage [] = "HTTP/1.1 403 Forbidden\n\r\n<html>\n\n<title>\nSimple URL Censorship Error Page for CPSC 441 Assignment 1\n</title>\n\n<body>\n\n<h1>NO!!</h1>\n\n<p>\nSorry, but the Web page that you were trying to access\nhas a <b>URL</b> that is inappropriate.\n</p>\n<p>\nThe page has been blocked to avoid insulting your intelligence.\n</p>\n\n<p>\nWeb Censorship Proxy\n</p>\n\n</body>\n\n</html>";


    //main socket for browser interaction
    server.sin_family = AF_INET; //family type
    server.sin_addr.s_addr = INADDR_ANY; //bind to ip
    server.sin_port = htons (PORTMAIN); //port number of server

    //create main socket
    parentSocketDesc = socket (PF_INET, SOCK_STREAM, 0);    //PF_INET is family type, SOCK_STREAM is connection stream, protocol 0

    //can't create main socket
    if (parentSocketDesc == -1) { 
        puts("Can't create main socket.");
        return 1;
    }

    //attempts to binds main socket to specific address and port, returns -1 when unable to bind
    if (bind(parentSocketDesc, (struct sockaddr *)&server, sizeof(server)) == -1){ 
        puts("Can't bind main socket to specific address and port.");
        return 1;
    }
    else {
      puts("Successfully binded for main socket.");  
    }
    
    //attempts to put main socket described by parentSocketDesc in listening mode; (max queue of 10); if listen() returns -1, error in call to listen()
    if (listen(parentSocketDesc, 10) == -1){
        puts("Failed to put main socket in listening mode.");
        return 1;
    }

    //socket for telnet requests
    server.sin_family = AF_INET; //family type
    server.sin_addr.s_addr = INADDR_ANY; //bind to ip
    server.sin_port = htons (PORTTELNET); //port number of server

    //setting timer for accepting a connection for telnet socket
    timer.tv_sec = 30; //30 seconds
    timer.tv_usec = 0; //0 microseconds

    //create telnet socket
    telnetParentDesc = socket(PF_INET, SOCK_STREAM, 0);

    //can't create telnet socket
    if (telnetParentDesc == -1) { 
        puts("Can't create telnet socket.");
        return 1;
    }

    //setting a timeout operation for the socket, SOL_SOCKET sets operation at socket level, SO_RCVTIMEO refers to setting a timer on an input
    if (setsockopt (telnetParentDesc, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timer, sizeof timer) < 0){
        puts("Error in setsockopt()");
        return 1;
    }

    //binds telnet socket to specific address and port
    if (bind(telnetParentDesc, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) == -1){ 
        puts("Can't bind telnet socket to specific address and port.");
        return 1;
    } 
    else {
        puts("Successfully binded for telnet socket.");
    }

    //attempts to put telnet socket in listening mode (max queue of 10)
    if (listen(telnetParentDesc, 10) == -1){
        puts("Failed to put socket in listening mode.");
        return 1;
    }

    puts("Please connect to server with port 43322 in 30 seconds.");
    //attempt to accept a new connection on telnetParentDesc (telnet socket) with 30 second timer
    if((telnetChildDesc = accept(telnetParentDesc, NULL, NULL)) == -1) {
        //fail to accept a new connection to main socket
        puts ("Failed to accept a new connection from telnet.");
        return 1;
    }
    else {
        puts("Accepted a connection from telnet.");
        timer.tv_sec = 1; 
        timer.tv_usec = 0;
        //set timer for 1 second on telnet socket, stops receiving after 1 second, then skips recv till it reaches it again
        if (setsockopt (telnetChildDesc, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timer, sizeof timer) < 0){
            puts("Error in setsockopt()");
            return 1;
        }
    }

    puts("Telnet socket has been connected");
    close(telnetParentDesc);

    puts("Waiting for browser connections...");

    c = sizeof(struct sockaddr_in);             //size of sockaddr_in structure

    bzero(browserRequest, MESSAGELENGTH);       //erases data for browserRequest
    bzero(webServerReply, MESSAGELENGTH);       //erases data for webServerReply
    bzero(telnetRequest, MESSAGELENGTH);        //erases data for telnetRequest

    //keeps accepting new connections until program is terminated
    for ( ; ; ){
        puts("Top of for loop.");
        //attempt to receive a request from telnet and put into telnetRequest with size treqSize, this times out after 1 second
        //this times out after 1 second
        treqSize = recv(telnetChildDesc, telnetRequest, MESSAGELENGTH, 0);

        //check telnet request "BLOCK" command
        if (telnetRequest[0] == 'B' && telnetRequest[1] == 'L' && telnetRequest[2] == 'O' && telnetRequest[3] == 'C' && telnetRequest[4] == 'K' && telnetRequest[5] == ' '){
            if (blockCounter == 5){
                //send a message when block list is full
                char counterMessage [] = "Can't block anymore words.\n";
                if(send(telnetChildDesc, counterMessage, strlen(counterMessage), 0 ) < 0){
                    puts("Can't send counter message to client.");
                }
            }
            else {
                //parsing to get word after BLOCK command
                char* wordBlock = strstr(telnetRequest, " ");       //find space after BLOCK
                wordBlock ++;                                       //points to char after space
                *strchr(wordBlock, '\r') = '\0';                    //sets last char to \0
                strcpy(blockList[blockCounter], wordBlock);         //copy into list
                char blockedMessage [] = "Added to block list.\n";  //message to be sent to telnet
                blockCounter ++;                                    //increment block counter to keep track of how many words are in list
                if(send(telnetChildDesc, blockedMessage, strlen(blockedMessage), 0 ) < 0){
                    puts("Can't send counter message to client.");
                }
            }
            bzero(telnetRequest, MESSAGELENGTH);                    //erase data for telnetRequest
        }
        //chech telnet request for "UNBLOCK" command
        else if (telnetRequest[0] == 'U' && telnetRequest[1] == 'N' && telnetRequest[2] == 'B' && telnetRequest[3] == 'L' && telnetRequest[4] == 'O' && telnetRequest[5] == 'C' && telnetRequest[6] == 'K'){
            char unblockMessage [] = "Clearing block list.\n";      //message to be sent to telnet
            //erases all data in block list and sets blockCounter back to 0
            bzero(blockList[0], BLOCKMESSAGELENGTH);
            bzero(blockList[1], BLOCKMESSAGELENGTH);
            bzero(blockList[2], BLOCKMESSAGELENGTH);
            bzero(blockList[3], BLOCKMESSAGELENGTH);
            bzero(blockList[4], BLOCKMESSAGELENGTH);
            blockCounter = 0;
            if(send(telnetChildDesc, unblockMessage, strlen(unblockMessage), 0 ) < 0){
                    puts("Can't send unblock message to client.");
            }
            bzero(telnetRequest, MESSAGELENGTH);
        }

        //attempt to accept a new connection on parentSocketDesc (main socket)
        childSocketDesc = accept(parentSocketDesc, (struct sockaddr *)&client, (socklen_t *) &c);
        //fail to accept a new connection to main socket
        if(childSocketDesc == -1 ) {
            //Error in call to accept
            puts ("Failed to accept a new connection from browser.");
            return 1;
        }
        else {
            puts("Accepted a connection from browser.");
        }
        
        //attempt to receive a request from browser and put into browserRequest with size breqSize
        //if nothing is being sent, go back to top of loop to refresh
        if ((breqSize = recv(childSocketDesc, browserRequest, MESSAGELENGTH, 0)) == 0){
           puts("breqSize is 0.");
           continue;  
        }

        //failed to receive a request
        if (breqSize == -1){
            puts("Failed to receive a request from the browser.");
        }

        puts(browserRequest);

        //looks for "POST" keyword in the request, goes back to top of the loop
        if (browserRequest[0] == 'P' && browserRequest[1] == 'O' && browserRequest[2] == 'S' && browserRequest[3] == 'T'){
            continue;
        }

        //parse GET request to get host name
        char *chunk1 = browserRequest;                          //points to browserRequest
        chunk1 += 11;                                           //points to (pages.cpsc....) "GET http://" has 11 chars
        char *chunk2 = strstr(chunk1, " ");                     //finds "HTTP/1.1" and points to space before it
        int urlLength = strlen(chunk1) - strlen(chunk2);        //finds length of URL
        char URL [MESSAGELENGTH];                               //URL with max size of MESSAGELENGTH
        memcpy(URL, chunk1, urlLength);                         //sets URL from GET request
        hostName = strtok(URL, "/");                            //finds first "/", sets hostName to everything before "/"
       
        puts(URL);  

        char *webServerRequest;                                             //request being sent to web server
        //check if url needs to be blocked
        if (blockCounter != 0){                                             //block counter is not empty
            for (int i = 0; i < blockCounter; i++){                         //iterate through blockList
                char *findKeyWord = strstr(browserRequest, blockList[i]);   //finds keyword in browserRequest, returns NULL if word is not in request
                if (findKeyWord == 0){
                    webServerRequest = browserRequest;                      //if NULL sets webServerRequest to browserRequest
                }
                else {
                    webServerRequest = errorRequest;                        //if not NULL sets webServerRequest to errorRequest
                    puts("This needs to be blocked.");                      
                    break;
                }
            }
        }
        else {
            webServerRequest = browserRequest;                              //if block counter is empty, dont need to block, sets webServerRequest as browserRequest
            puts("This does not need to be blocked.");
        }
        
        //create client-side proxy socket to connect to web server
        clientProxyDesc = socket(PF_INET, SOCK_STREAM, 0);

        //can't create client-side proxy
        if (clientProxyDesc == -1){
            puts("Can't create client-side proxy.");
        }

        //gets address of hostName and returns a hostent structure
        address = gethostbyname(hostName);

        clientProxy.sin_family = AF_INET;                                                           //family type
        bcopy ((char*)address->h_addr, (char*) &clientProxy.sin_addr.s_addr, address->h_length);    //sets ip of proxy
        clientProxy.sin_port = htons (80);                                                          //http port

        //connect client-side socket referered by clientProxyDesc to address of clientProxy structure (hostIP);
        if (connect(clientProxyDesc, (struct sockaddr *)&clientProxy, sizeof(clientProxy)) == -1){
            puts("Can't connect to web server.");
            return 1;
        }
        puts("Connected to web server.");
        puts(webServerRequest);

        //attempt to send webServerRequest to socket described by clientProxyDesc
        sreqSize = send(clientProxyDesc, webServerRequest, strlen(webServerRequest), 0);
        //failed to send webServerRequest to socket described by clientProxyDesc
        if (sreqSize == -1){
            puts("Failed to send a request to web server.");
        }
        puts ("Request sent to web server.");

        int flag = 0;       //represents as a boolean, flag = 1 when html body has word in block list

        //attempts to receive web server reply into webServerReply with size srepSize
        //in a while loop to handle images with larger replies
        while ((srepSize = recv(clientProxyDesc, webServerReply, MESSAGELENGTH, 0)) > 0){
            puts("Reply receied from web server.");
            puts(webServerReply);

            //check if url needs to be blocked by going through the webServerReply
            if (blockCounter != 0){
                for (int i = 0; i < blockCounter; i++){                         //iterates through blockList
                    char *findKeyWord = strstr(webServerReply, blockList[i]);   //finds keyword in webServerReply, returns NULL if word is not in request
                    if(findKeyWord != 0){                                       //keyword is found
                        puts("Found bad word in html code.");                   
                        flag = 1;                                               //keyword is found, sets flag to 1
                    }
                }
            }
            if (flag == 1 ){                                                            //need to block
                //attempts to send error page to browser
                if (send(childSocketDesc, errorPage, strlen(errorPage), 0) == -1){      
                    puts("Failed to send error page to web browser.");
                }
                puts("Sent error page to web browser.");
            }
            else {                                                                      //don't need to block
                //attempts to send original webServerReply to browser
                if (send(childSocketDesc, webServerReply, srepSize, 0) == -1){
                    puts("Failed to send web server reply to web browser.");
                }
                puts("Sent web server reply to web browser.");
            }
        }

        bzero(browserRequest, MESSAGELENGTH);       //erases data in browserRequest
        bzero(webServerReply, MESSAGELENGTH);       //erases data in webServerReply
        bzero(telnetRequest, MESSAGELENGTH);        //erases data in telnetRequest
        close(clientProxyDesc);                     //closes socket that connects to web server
        close(childSocketDesc);                     //closes socket that connects to web browser
    }                                               //back to top of loop
    return 0;
}