#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include "./utilities.h"

int main(void){
    struct msgbuf buf;
    int msgqid;
    key_t key = getMsgQKey();

    int sequence_number, operation_number, shmid, *shmPtr, node;
    char file_name[10];
    if ((msgqid = msgget(key, PERMS)) == -1){
        perror("msgget");
        exit(1);
    }
    int msg_len=0;

    while(1){
        printf("\n::Available Operation Numbers::\n");
        printf ("Enter 1 to Add a new graph to the database\n");
        printf ("Enter 2 Modify an existing graph of the database\n");
        printf ("Enter 3 to Perform DFS on an existing graph of the database\n");
        printf ("Enter 4 to Perform BFS on an existing graph of the database\n");

        printf ("Enter Sequence Number\n");
        scanf("%d",&sequence_number);

        printf ("Enter Operation Number\n");
        scanf("%d",&operation_number);
        if(operation_number < 1 || operation_number > 4){
          printf("Invalid Operation Number\n");
          continue;
        }

        printf ("Enter Graph File Name\n");
        scanf("%s",file_name);

        msg_len = sprintf(buf.mtext, "%d %d %s", sequence_number, operation_number, file_name) + 1;
        buf.mtype  = LOAD_BALANCER;
        if(msgsnd(msgqid, &buf, msg_len, 0) == -1){
			perror("msgsnd");
			exit(1);
	    }


            // NOTE: We could make request specific SHM_BUF sizes (2*sizeof(int) for read and (2+n*n)*sizeof(int) for writes), but the memory saved is considered insignificant for this assignment
            shmid=shmget(sequence_number, SHM_BUF_SIZE, PERMS|IPC_CREAT);
            if(shmid==-1)
            {
                perror("error in shmget\n");
                exit(-1);
            }
            shmPtr= shmat(shmid,NULL,0);
            if(shmPtr==(void*)-1)
            {
                perror("shared memory error\n");
                exit(-2);
            }
        if(operation_number==1 || operation_number ==2){

            printf ("Enter number of nodes of the graph\n");
            int number_of_nodes;
            scanf("%d",&number_of_nodes);

            // write number of nodes to shared memory
            shmPtr[1] = number_of_nodes;

            printf ("Enter adjacency matrix, each row on a separate line and elements of a single row separated by whitespace characters\n");

            for(int i=2;i<=number_of_nodes*number_of_nodes + 1;i++){
                scanf("%d",&node);
                shmPtr[i]=node;
            }
            // SYNC: shmPtr[0] = 1 implies data has been successfully written to shared memory, the server thread can start reading now
            shmPtr[0] = 1;
        }
        else if(operation_number==3 || operation_number ==4){
            printf("Enter starting vertex\n");
            int starting_vertex;
            scanf("%d",&starting_vertex);
            shmPtr[1]=starting_vertex;
            // SYNC: shmPtr[0] = 1 implies data has been successfully written to shared memory, the server thread can start reading now
            shmPtr[0] = 1;
        }

        printf("Client waiting for reply...\n");
        // Flag to truncate msg if its size is longer than the buffer (mtext)
        memset(buf.mtext, 0, 1000);
        buf.mtype = sequence_number; //doubt
        if(msgrcv(msgqid, &buf, sizeof(buf.mtext), sequence_number, MSG_NOERROR)==-1){
            perror("msgrcv-Server has been Terminated\n");
            break;
        }
        // Print Server's reply
        printf("Reply recieved: \"%s\"\n", buf.mtext);



            if(shmdt(shmPtr)== -1){
                perror("error in detaching\n");
                exit(-6);
            
            }
            if(shmctl(shmid,IPC_RMID,0) == -1){
                perror("error in shmctl");
                exit(-7);
            }



    }



    return 0;
}