#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include "./utilities.h"

int main(void) {  
  /**** Initializing variables used *****/
  struct msgbuf buf;
  int msgqid;
  key_t key = getMsgQKey();
  // to save the input by user
  char input;

  // get the queue details
  if ((msgqid = msgget(key, PERMS)) == -1){
    perror("msgget");
    exit(1);
  }
  
  // save the server termination message
  buf.mtype = LOAD_BALANCER;
  buf.mtext[0] = ')';
  buf.mtext[1] = '\0';

  // Infinite loop for the cleanup process
  while(1){
    printf("Want to terminate the application? Press Y (Yes) or N (No): ");
    scanf("%c", &input);
    if (input == 'Y'){
      printf("Terminating the server\n");
      if (msgsnd(msgqid, &buf, 2, 0) == -1){
        perror("msgsnd");
        exit(1);
      }
      break;
    }
    scanf("%c", &input);
  }
  return 0;
}
