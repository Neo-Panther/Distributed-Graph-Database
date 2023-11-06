#ifndef UTILITIES_H
#define UTILITIES_H
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

// shared memory buffer size
#define SHM_BUF_SIZE 901*sizeof(int)

// named semaphores' names
#define WRITE_SEMAPHORE "/write"
#define READ_SEMAPHORE "/read"

// ipc permissions
#define PERMS 0600
// msg structure for communication
struct msgbuf {
  long mtype;
  char mtext[1000];
};

// returns the single msgq key
key_t getMsgQKey(){
  key_t key;
  if ((key = ftok("load_balancer.c", 'A')) == -1){
    perror("ftok");
    exit(1);
  }
  return key;
}

// Utility function used to parse input from client
// get index of next space
int get_next_space(char *s, int start){
  int i = start;
  while(s[i]!=' '){
    i++;
  }
  return i+1;
}

#define LOAD_BALANCER 1023
#define SECONDARY_SERVER_2 1022
#define SECONDARY_SERVER_1 1021
#define PRIMARY_SERVER 1020
#endif