#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAXDATASIZE 1024 // max buffer size
#define SERVER_PORT 2001
// gcc client.c -o client -std=c99 -lpthread -lreadline 
// ./client localhost
// ctrl+c, enter, to exit

int client_socket(char *hostname)
{
    char port[20];
    struct addrinfo their_addrinfo; // server address info
    struct addrinfo *their_addr = NULL; // connector's address information
    int sockfd;

    int n = snprintf(port, 20, "%d", SERVER_PORT); // Make a string out of the port number
    if ((n < 0) || (n >= 20))
    {
        printf("ERROR: Malformed Port\n");
        exit(EXIT_FAILURE);
    }
    printf("port: %s.\n", port);

    // 1) initialise socket using 'socket'
    int domain = AF_INET;//AF_UNIX;
    int type = SOCK_STREAM;//SOCK_DGRAM
    if((sockfd = socket(domain, type, 0)) == -1){
      perror("ERROR: socket failed to init.\n");
      exit(EXIT_FAILURE);
    }
    // 2) initialise 'their_addrinfo' and use 'getaddrinfo' to lookup the IP from host name
    memset(&their_addrinfo,0,sizeof(struct addrinfo));
    their_addrinfo.ai_family = AF_INET;//server
    their_addrinfo.ai_socktype = SOCK_STREAM;

    // memset()
    int error = getaddrinfo(hostname, port, &their_addrinfo, &their_addr);
    if(error != 0){
      printf(": getaddrinfo error %d.\n", error);
      exit(-1);
    }else{
      printf(".\n");
    }
    // getaddrinfo

    // 3) connect to remote host using 'connect'
    int rc = connect(sockfd, their_addr->ai_addr, their_addr->ai_addrlen);
    if ( rc == -1){
       printf(": socket connection error");
       exit(-1);
    }

    return sockfd;
}


// thread function to handle reading received messages to output
void* handle_write(void* args){//arg = fd to read from
    int sockfd = *((int*)args);//cast as int ptr, than de-reference to int
    char *line;
    while(1) {
      line = readline(">> ");
      write(sockfd, line, strlen(line));
    }

    free(line);
    close(sockfd);
    pthread_exit(NULL);
}

// thread function to handle reading received messages to output
void* handle_read(void* args){//arg = fd to read from    
    int sockfd = *((int*)args);//cast as int ptr, than de-reference to int
    int numbytes = 0;
    char buffer[MAXDATASIZE];
    do {
        numbytes = read(sockfd, buffer, MAXDATASIZE - 1);
        buffer[numbytes] = '\0';
        printf("server: %s\n", buffer);
    } while(numbytes > 0);

    close(sockfd);
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{

    if (argc != 2) {
        fprintf(stderr, "usage: client hostname\n");
        exit(1);
    }

    int sockfd = client_socket(argv[1]);

    pthread_t writeThread;
    pthread_create(&writeThread, NULL, handle_write, &sockfd);

    pthread_t readThread;
    pthread_create(&readThread, NULL, handle_read, &sockfd);
    
    pthread_join(writeThread, NULL);
    pthread_join(readThread, NULL);
    close(sockfd);

    return 0;
}