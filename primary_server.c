#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include "./utilities.h"

/**Initializing global variables (shared with threads)**/
// msg q vars
struct msgbuf buf;
int msgqid;
key_t key;
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

  int shmid = shmget(sequence_number, SHM_BUF_SIZE, PERMS);
  if (shmid==-1){
    perror("shmget");
    exit(1);
  }
  // attach shared memory to thread address space for reading
  int *shmPtr = shmat(shmid, NULL, SHM_RDONLY);
  if(shmPtr == (void*)-1){
    perror("shmat");
    exit(1);
  }

  // counter - denotes the curent (relative) location of read pointer in the shm
  int counter = 0;
  // SYNC: wait until client is done writing to the shared memory
  while(shmPtr[counter] != 1);
  counter++;
  // TODO: SYNC: obtain named write semaphore
  
  // open the file in write and read mode (it is created if it doesn't exist)
  // the files contents are cleared if it exists
  FILE* file = fopen(file_name, "w");
  if(file == NULL){
    perror("fopen");
    exit(1);
  }
  int number_of_nodes = shmPtr[counter++];

  // put the number of nodes on the first line
  if (fprintf(file, "%d\n", number_of_nodes) < 0){
    perror("fprintf-1");
    exit(1);
  }

  // put the adjacency matrix
  for(int i = 1; i <= number_of_nodes; i++){
    if(fprintf(file, "%d", shmPtr[counter++]) < 0){
      perror("fprintf-2");
      exit(1);
    }
    for(int j = 2; j <= number_of_nodes; j++){
      if(fprintf(file, " %d", shmPtr[counter++]) < 0){
        perror("fprintf-3");
        exit(1);
      }
    }
    // skip newline on the last line
    if(i != number_of_nodes){
      if(fprintf(file, "\n") < 0){
        perror("fprintf-4");
        exit(1);
      }
    }
  }

  // close the file and flush the buffer (only the user space buffer, not the kernel buffers - write may not happen if you switch off the)
  if(fclose(file) != 0){
    perror("fclose");
    exit(1);
  }
  // TODO: SYNC: release the named write semaphore

  // detach the shmPtr, ! destorying shm is the work of client
  if(shmdt(shmPtr) == -1){
    perror("shmdt");
    exit(1);
  }

  // send operation output to client
  // create message depending on operation number
  int msg_len = ((operation_number == 1) ? sprintf(buf.mtext, "File successfully added") : sprintf(buf.mtext, "File successfully modified")) + 1;
  buf.mtype  = sequence_number;
  if(msgsnd(msgqid, &buf, msg_len, 0) == -1){
    perror("msgsnd");
    exit(1);
  }
  return NULL;
}

int main(void){
  /**Initializing Variables**/
  // thread id variable
  pthread_t tid;
  // thread inp variable (pointer)
  char *tin;
  
  // tids index counter
  int tidx = 0;
  /**************************/
  
  //get msg q details
  key = getMsgQKey();
  if ((msgqid = msgget(key, PERMS)) == -1){
    perror("msgget");
    exit(1);
  }

  while(1){
    // listen to requests from the load balancer
    memset(buf.mtext, 0, sizeof(buf.mtext));
    printf("\nPrimary Server Waiting for messages from Load Balancer...\n");
    // receive a message, truncate it if longer than buf.mtext's size
    if (msgrcv (msgqid, &buf, sizeof(buf.mtext), PRIMARY_SERVER, MSG_NOERROR) == -1){
      perror ("msgrcv");
      pthread_exit((void*)1);
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
    if(pthread_create(&tid, NULL, writer, &tin) != 0){
      perror("pthread_create");
      pthread_exit((void*)1);
    }
  }
  // no cleanup required by the server
  // wait for all threads to terminate before exiting
  pthread_exit(NULL);
}