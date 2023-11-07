#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/shm.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdint.h>
#include "./utilities.h"

// TODO: handle starvation of readers by using sequence number ordering => allow readers before writers if their sequence number is lower

/**Declaring global variables (shared between all threads)**/
// msg q vars
struct msgbuf buf;
int msgqid;
key_t key;

// variables for synchronization between threads

/*******************************************************/

void *writer(void* args){
  // get input in correct format
  char* tin = (char*) args;
  
  // extract input from tin
  int sequence_number = atoi(tin);
  // get the index of the next space
  int nexti = get_next_space(tin, 0);

  int operation_number = atoi(tin+nexti);
  char file_name[10];
  strcpy(file_name, tin+get_next_space(tin, nexti));
  // free the input struct after reading it
  free(args);
  printf("Writer Thread received input>sno:%d:opno:%d:fname:%s:\n", sequence_number, operation_number, file_name);

  int shmid = shmget(sequence_number, SHM_BUF_SIZE, PERMS);
  if (shmid==-1){
    perror("shmget");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }
  // attach shared memory to thread address space for reading
  int *shmPtr = shmat(shmid, NULL, SHM_RDONLY);
  if(shmPtr == (void*)(intptr_t)-1){
    perror("shmat");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }

  // counter - denotes the curent (relative) location of read pointer in the shm
  int counter = 0;
  // SYNC: busy wait until client is done writing to the shared memory
  while(shmPtr[counter] != 1);
  counter++;
  int number_of_nodes = shmPtr[counter++];
  printf("Writer Thread received number of nodes: %d\n", number_of_nodes);
  // SYNC: Writer <Entry Section> Starts
  
  // <Entry Section> Ends
  // <CRITICAL SECTION> starts
  // open the file in write mode (it is created if it doesn't exist)
  // the files contents are cleared if it exists
  FILE* file = fopen(file_name, "w");
  if(file == NULL){
    perror("fopen");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }

  // put the number of nodes on the first line
  if (fprintf(file, "%d\n", number_of_nodes) < 0){
    perror("fprintf-1");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }

  // put the adjacency matrix
  for(int i = 1; i <= number_of_nodes; i++){
    printf("Thread writing %d\n", shmPtr[counter]);
    if(fprintf(file, "%d", shmPtr[counter++]) < 0){
      perror("fprintf-2");
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
    for(int j = 2; j <= number_of_nodes; j++){
      printf("Thread writing %d\n", shmPtr[counter]);
      if(fprintf(file, " %d", shmPtr[counter++]) < 0){
        perror("fprintf-3");
        pthread_exit((void*)(intptr_t)EXIT_FAILURE);
      }
    }
    // skip newline on the last line
    if(i != number_of_nodes){
      if(fprintf(file, "\n") < 0){
        perror("fprintf-4");
        pthread_exit((void*)(intptr_t)EXIT_FAILURE);
      }
    }
  }

  // close the file and flush the buffer (only the user space buffer, not the kernel buffers - write may not happen if you switch off the)
  if(fclose(file) != 0){
    perror("fclose");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }

  // <CRITICAL SECTION> ends
  // SYNC: Writer <Exit Section>

  // <Exit Section> Ends

  // detach the shmPtr, ! destorying shm is the work of client
  if(shmdt(shmPtr) == -1){
    perror("shmdt");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }

  // send operation output to client
  // create message depending on operation number
  printf("Writer done writing-sending output: ");
  int msg_len = ((operation_number == 1) ? sprintf(buf.mtext, "File successfully added") : sprintf(buf.mtext, "File successfully modified")) + 1;
  printf("%s\n", buf.mtext);
  buf.mtype  = sequence_number;
  if(msgsnd(msgqid, &buf, msg_len, 0) == -1){
    perror("msgsnd");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }
  return (void*)(intptr_t)EXIT_SUCCESS;
}

int main(void){
  /**Declaring main thread Local Variables**/
  // thread id variable
  pthread_t tid;
  // thread inp variable (pointer)
  char *tin;
  /*****************************************/

  /******Initializing Msg Q Variables*******/
  key = getMsgQKey();
  if ((msgqid = msgget(key, PERMS)) == -1){
    perror("msgget");
    exit(EXIT_FAILURE);
  }
  /*****************************************/

  /******Initializing Semaphores and Mutexes*******/

  /*****************************************/

  while(1){
    // listen to requests from the load balancer
    // set all bytes of buf.mtext to 0 for next message
    memset(buf.mtext, 0, sizeof(buf.mtext));
    printf("\nPrimary Server Waiting for messages from Load Balancer...\n");
    // receive a message, truncate it if longer than buf.mtext's size
    if (msgrcv (msgqid, &buf, sizeof(buf.mtext), PRIMARY_SERVER, MSG_NOERROR) == -1){
      perror ("msgrcv");
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
    printf("Primary Server Message received: %s\n", buf.mtext);

    // check if it is a termination message
    if (buf.mtext[0] == ')'){
      printf("Primary Server Terminating...\n");
      break;
    }

    // allocate new space every time (to preserve other threads input) - free inside thread subroutine
    tin = (char*)calloc(20, sizeof(char));

    // set required values in tin
    strcpy(tin, buf.mtext);

    // create thread with thread id in tid (overwritten in tid), default attributes, write subroutine and tin as its input
    printf("Primary Server Sending input to thread:%s\n", tin);
    if(pthread_create(&tid, NULL, writer, tin) != 0){
      perror("pthread_create");
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
  }
  // unlink named semaphores and destroy mutex

  // wait for all threads to terminate before exiting
  pthread_exit((void*)(intptr_t)EXIT_SUCCESS);
}