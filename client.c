#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include "./utilities.h"

int main(void){
  /**Initializing Variables**/
  // message queue variables
  struct msgbuf buf;
  int msgqid;
  key_t key = getMsgQKey();
  int msg_len;

  // shared memory variables
  int sequence_number, operation_number, shmid, *shmPtr, node;
  char file_name[10];
  /**************************/
  if ((msgqid = msgget(key, PERMS)) == -1){
    perror("msgget");
    exit(EXIT_FAILURE);
  }

  // client infinite loop - terminate by Ctrl+C
  while(1){
    printf("\n::Available Operation Numbers::\n");
    printf ("Enter 1 to Add a new graph to the database\n");
    printf ("Enter 2 Modify an existing graph of the database\n");
    printf ("Enter 3 to Perform DFS on an existing graph of the database\n");
    printf ("Enter 4 to Perform BFS on an existing graph of the database\n");
    printf ("-----------------------------------------------------------------\n");
    // get common user input
    printf ("Enter Sequence Number\n");
    scanf("%d",&sequence_number);
    printf ("Enter Operation Number: ");
    scanf("%d",&operation_number);
    if(operation_number < 1 || operation_number > 4){
      printf("Invalid Operation Number: ");
      continue;
    }
    printf ("Enter Graph File Name: ");
    scanf("%s",file_name);

    msg_len = sprintf(buf.mtext, "%d %d %s", sequence_number, operation_number, file_name) + 1;
    buf.mtype  = LOAD_BALANCER;
    if(msgsnd(msgqid, &buf, msg_len, 0) == -1){
      perror("msgsnd-Check if the load balancer is running");
      exit(EXIT_FAILURE);
    }

    // NOTE: We could make request specific SHM_BUF sizes (2*sizeof(int) for read and (2+n*n)*sizeof(int) for writes), but the memory saved is considered insignificant for this assignment
    // shared memory key = unique sequence number of each request
    if((shmid=shmget(sequence_number, SHM_BUF_SIZE, PERMS|IPC_CREAT))==-1){
      perror("shmget");
      exit(EXIT_FAILURE);
    }
    if((shmPtr= shmat(shmid,NULL,0))==(void*)(intptr_t)-1){
      perror("shmat");
      exit(EXIT_FAILURE);
    }
    if(operation_number==1 || operation_number ==2){
      // get and write number of nodes and adj_mat to shared memory - send to primary servers
      printf ("Enter number of nodes of the graph: ");
      int number_of_nodes;
      scanf("%d",&number_of_nodes);

      shmPtr[1] = number_of_nodes;

      printf ("Enter adjacency matrix\nEach row on a separate line\nElements of a single row separated by single space characters::\n");

      for(int i=2;i<=number_of_nodes*number_of_nodes + 1;i++){
          scanf("%d",&node);
          shmPtr[i]=node;
      }
      // SYNC: shmPtr[0] = 1 implies data has been successfully written to shared memory, the server thread can start reading now
      shmPtr[0] = 1;
    } else if(operation_number==3 || operation_number ==4){
      // get user input and write to shared memory - send to servers
      printf("Enter starting vertex: ");
      int starting_vertex;
      scanf("%d",&starting_vertex);
      shmPtr[1]=starting_vertex;
      // SYNC: shmPtr[0] = 1 implies data has been successfully written to shared memory, the server thread can start reading now
      shmPtr[0] = 1;
    }

    printf("Client waiting for reply...\n");
    // Flag to truncate msg if its size is longer than the buffer (mtext)
    memset(buf.mtext, 0, 1000);
    buf.mtype = sequence_number;
    // wait for and get server's reply
    if(msgrcv(msgqid, &buf, sizeof(buf.mtext), sequence_number, MSG_NOERROR)==-1){
      perror("msgrcv-Server has been Terminated\n");
      exit(EXIT_FAILURE);
    }
    // Print Server's reply
    printf("Reply recieved: \"%s\"\n", buf.mtext);

    // detach and destroy the shm
    if(shmdt(shmPtr)== -1){
      perror("shmdt");
      exit(EXIT_FAILURE);
    }
    if(shmctl(shmid,IPC_RMID,0) == -1){
      perror("shmctl");
      exit(EXIT_FAILURE);
    }
  }
  return EXIT_SUCCESS;
}