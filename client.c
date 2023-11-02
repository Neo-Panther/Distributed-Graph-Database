#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include "./utilities.h"
#define BUF_SIZE 901*sizeof(int)

int main(void){
    struct msgbuf buf;
    int msgqid;
    key_t key = getMsgQKey();


    if ((msgqid = msgget(key, PERMS)) == -1){
        perror("msgget");
        exit(1);
    }
    int msg_len=0;

    while(1){
        printf("\n\n::Available Task Numbers::\n");
        printf ("Enter 1 to Add a new graph to the database\n");
        printf ("Enter 2 Modify an existing graph of the database\n");
        printf ("Enter 3 to Perform DFS on an existing graph of the database\n");
        printf ("Enter 4 to Perform BFS on an existing graph of the database\n");

        char task_input[976];

        printf ("Enter Sequence Number\n");
        int sequence_number;
        scanf("%d",&sequence_number);

        printf ("Enter Operation Number\n");
        int operation_number;
        scanf("%d",&operation_number);

        printf ("Enter Graph File Name\n");
        char file_name[200];
        scanf("%s",file_name);

        msg_len = sprintf(buf.mtext, "%d %d %s", sequence_number, operation_number, file_name) + 1;
        buf.mtype  = LOAD_BALANCER;
        if(msgsnd(msgqid, &buf, strlen(buf.mtext) + 1, 0) == -1){
			perror("msgsnd");
			exit(1);
	    }

            int shmid,*shmPtr;

            shmid=shmget(sequence_number,BUF_SIZE,0644|IPC_CREAT);
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

            printf ("Enter adjacency matrix, each row on a separate line and elements of a single row separated by whitespace characters\n");

            int node;
            for(int i=0;i<number_of_nodes*number_of_nodes;i++){
                scanf("%d",&node);
                shmPtr[i]=node;
            }
        }
        else if(operation_number==3 || operation_number ==4){
            printf("Enter starting vertex\n");
            int starting_vertex;
            scanf("%d",&starting_vertex);
            shmPtr[0]=starting_vertex;
            
        }
        else {

            printf("Invalid Task Number: ");
        }

        printf("Client waiting for reply...\n");
        // Flag to truncate msg if its size is longer than the buffer (mtext)
        memset(buf.mtext, 0, 1000);
        buf.mtype = sequence_number; //doubt
        if(msgrcv(msgqid, &buf, sizeof(buf.mtext), sequence_number, 0)==-1){
            printf("Server has been Terminated\n");
            break;
        }
        // Print Server's reply
        printf("Reply recieved: \"%s\"\n", buf.mtext);



            if(shmdt(shmPtr)== -1){
                printf("error in detaching\n");
                exit(-6);
            
            }
            if(shmctl(shmid,IPC_RMID,0) == -1){
                perror("error in shmctl");
                exit(-7);
            }



    }



    return 0;
}