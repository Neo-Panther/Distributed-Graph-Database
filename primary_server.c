#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/shm.h>
#include "./utilities.h"

void *write(void* args){
  // char * input
  // write to the graph after getting the write semaphore
}

int main(void){
  /**Initializing Variables**/
  struct msgbuf buf;
  int msgqid;
  key_t key = getMsgQKey();

  // variables to store msgq input
  int sequence_number, operation_number;
  char* graph_file_name;
  /**************************/
  //get msg q details
  if ((msgqid = msgget(key, PERMS)) == -1){
    perror("msgget");
    exit(1);
  }

  while(1){
    // listen to requests from the load balancer
    memset(buf.mtext, 0, sizeof(buf.mtext));
    printf("\nPrimary Server Waiting for messages from Load Balancer...\n");
    if (msgrcv (msgqid, &buf, sizeof(buf.mtext), PRIMARY_SERVER, MSG_NOERROR) == -1){
      perror ("msgrcv");
      exit(1);
    }
    printf("Primary Server Message received: %s\n", buf.mtext);

  }
  return 0;
}