#include <string.h>
#include <unistd.h>
#include "./utilities.h"

int main(void){
  printf("Load Balancer Started\n");
  /**Initializing Variables**/
  struct msgbuf buf;
  int msgqid;
  key_t key = getMsgQKey();

  // variables to store client msgs
  int sequence_number, operation_number;
  char* graph_file_name;
  /**************************/
  // Create Msg Q
  if ((msgqid = msgget(key, PERMS | IPC_CREAT)) == -1){
    perror("msgget");
    exit(1);
  }
  printf("Msg Q created");

  while(1){
    // listen to requests from client
    memset(buf.mtext, 0, sizeof(buf.mtext));
    printf("\nLoad Balancer Waiting for messages from client(s)...\n");
    if (msgrcv (msgqid, &buf, sizeof(buf.mtext), LOAD_BALANCER, MSG_NOERROR) == -1){
      perror ("msgrcv");
      exit(1);
    }
    printf("Load Balancer Message received: %s\n", buf.mtext);

    // check if it is a termination message
    if (buf.mtext[0] == ')'){
      printf("Load Balancer Terminating all servers with msg: %s\n", buf.mtext);
      buf.mtype = PRIMARY_SERVER;
      if(msgsnd(msgqid, &buf, strlen(buf.mtext) + 1, 0) == -1){
				perror("msgsnd");
				exit(1);
	    }
      buf.mtype = SECONDARY_SERVER_1;
      if(msgsnd(msgqid, &buf, strlen(buf.mtext) + 1, 0) == -1){
				perror("msgsnd");
				exit(1);
	    }
      buf.mtype = SECONDARY_SERVER_2;
      if(msgsnd(msgqid, &buf, strlen(buf.mtext) + 1, 0) == -1){
				perror("msgsnd");
				exit(1);
	    }
      sleep(5);  // NOTE: it would have been better to wait for an acknowledged from all servers before deleting the msgq, instead of sleep
      printf("Load Balancer Terminating\n");
      break;
    }
    sequence_number = atoi(buf.mtext);

    // get the index of the next space
    operation_number = atoi(buf.mtext+get_next_space(buf.mtext, 0));
 
    if(operation_number == 1 || operation_number == 2){ // Write
      printf("Load Balancer Sending Message to primary server: %s\n", buf.mtext);
      buf.mtype = PRIMARY_SERVER;
      if(msgsnd(msgqid, &buf, strlen(buf.mtext) + 1, 0) == -1){
				perror("msgsnd");
				exit(1);
	    }
    } else if(sequence_number%2) { // Read odd
      printf("Load Balancer Sending Message to secondary server 1: %s\n", buf.mtext);
      buf.mtype = SECONDARY_SERVER_1;
      if(msgsnd(msgqid, &buf, strlen(buf.mtext) + 1, 0) == -1){
				perror("msgsnd");
				exit(1);
	    }
    } else { // Read even
      printf("Load Balancer Sending Message to secondary server 2: %s\n", buf.mtext);
      buf.mtype = SECONDARY_SERVER_2;
      if(msgsnd(msgqid, &buf, strlen(buf.mtext) + 1, 0) == -1){
				perror("msgsnd");
				exit(1);
	    }
    }
  }

  // delete message queue (cleanup)
  if (msgctl(msgqid, IPC_RMID, NULL) == -1){
    perror("msgctl");
    exit(1);
  }
  // terminate load_balancer
  return 0;
}
