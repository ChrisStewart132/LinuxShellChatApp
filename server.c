#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>

#define MAXDATASIZE 1024 // message buffer length (bytes)
#define SERVER_PORT 2001
#define NUM_THREADS 16
// gcc server.c -o server -std=c99 -lpthread
// ./server

int listen_on(int port)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in sa;
    sa.sin_family = AF_INET;          /* communicate using internet address */
    sa.sin_addr.s_addr = INADDR_ANY;  /* accept all calls                   */
    sa.sin_port = htons(port);        /* this is port number                */

    int rc = bind(s, (struct sockaddr *)&sa, sizeof(sa)); /* bind address to socket   */
    if (rc == -1) { // Check for errors
        perror("bind");
        exit(1);
    }

    rc = listen(s, 5); /* listen for connections on a socket */
    if (rc == -1) {    // Check for errors
        perror("listen");
        exit(1);
    }

    return s;
}

// blocks with a bound server socket fd until a connetion is received
// returns a connection socket fd to use to comm with the client
int accept_connection(int sockfd) {
    struct sockaddr* addrPtr;// ptr to sockaddr
    socklen_t* addrlenPtr;
    int conn = accept(sockfd, NULL, NULL);
    if(conn < 0){
      perror("accept error.\n");
      exit(-1);
    }
    return conn;
}

// struct used by each thread
typedef struct {
	int* clientSockets;     // ptr to a global memory variable
	int i;                  // this threads index (used to close client socket when finished)
	pthread_mutex_t* lock;  // ptr to know when to access global variable
	pthread_t thread;       // local thread id
} Worker;


// thread callback function
void* handle_client(void* args){
    Worker* worker = (Worker*)args;
  
    // attempts to lock the global mutex else blocks until it can
    pthread_mutex_lock(worker->lock);// lock to read the global fd array
    int msgsock = worker->clientSockets[worker->i];// retrieve what fd this thread is handling
    pthread_mutex_unlock(worker->lock);

    char buffer[MAXDATASIZE];
    int num_read = 0;

    while(1) {
        num_read = read(msgsock, buffer, MAXDATASIZE - 1);// read a message from the client
        if(num_read <= 0){
            break;
        }
        buffer[num_read] = '\0';
        printf("    %d: %s \n", msgsock, buffer);

        // write received message to all connected clients
        pthread_mutex_lock(worker->lock);
        for(int i = 0; i < NUM_THREADS;i++){
            if(worker->clientSockets[i] != 0 && i != worker->i){      
                write(worker->clientSockets[i], &buffer, num_read);
                //printf("    %d: %s \n", worker->clientSockets[i], buffer);
            }
        } 
        pthread_mutex_unlock(worker->lock);
    }

    printf("connection closed with:%d\n", msgsock);
    close(msgsock);

    // reset the fd in the global array
    pthread_mutex_lock(worker->lock);
    worker->clientSockets[worker->i] = 0;
    pthread_mutex_unlock(worker->lock);

    pthread_exit(NULL);
}


int main(int argc, char *argv[]) {
    printf("\npid %d listening on port %d\n", getpid(), SERVER_PORT);

    // setup the server to bind and listen on a port
    int s = listen_on(SERVER_PORT);

    // create array of client sockets to store current clients (each with its own thread processing them)
    int clientSockets[NUM_THREADS];
    memset(clientSockets, 0, NUM_THREADS);

    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    Worker workers[NUM_THREADS];// each worker thread will be assigned a client and relay messages

    while (1) { 
        int msgsock = accept_connection(s); // block / wait for a client to connect
        printf("Received connection from client: %d\n", msgsock);

        // create a thread for the current accepted connection / client
        for(int i = 0; i < NUM_THREADS; i++){
            // find first free index slot to assign client and thread to
            if(clientSockets[i] == 0){
                clientSockets[i] = msgsock;
                Worker* worker = &workers[i];
                worker->lock = &lock;
                worker->clientSockets = clientSockets;
                worker->i = i;
                pthread_create(&workers[i].thread, NULL, handle_client, worker);
                break;
            }
        }
        
        // created thread will handle the new connected client until they close
          // main thread will continue loop listening for new connections

    }

    close(s);
    exit(0);
}