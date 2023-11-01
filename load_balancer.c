#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <pthread.h>

#define PERMS 0600
struct msgbuf {
  long mtype;
  char mtext[1000];
};

int main(void){
  printf("Load Balancer Started");
  /**Initializing Variables**/
  struct msgbuf buf;
  int msgqid;
  key_t key;
  /**************************/
  if ((key = ftok("load_balancer.c", 'A')) == -1){
    perror("ftok");
    exit(1);
  }
  if ((msgqid = msgget(key, PERMS | IPC_CREAT)) == -1){
    perror("msgget");
    exit(1);
  }
  printf("Msg Q created");

  while(1){
    // listen to requests from client
    memset(buf.mtext, 0, 1000);
    printf("\n(Server) Waiting for messages from client(s)...\n");
    if (msgrcv (msgqid, &buf, sizeof(buf.mtext), 640, 0) == -1){
      perror ("msgrcv");
      exit(1);
    }
  }
}
