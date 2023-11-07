#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/shm.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdint.h>
#include "./utilities.h"

/**Declaring global variables (shared between all threads)**/
// msg q vars
struct msgbuf buf;
int msgqid;
key_t key;

// variables for synchronization between threads
/*******************************************************/

// struct to send input for graph traversal
struct dbfs_input {
  int number_of_nodes;
  int** adj_matrix;
  int current_vertex;
  int* output_array;
  int output_index;
  int* visited;
};

void* dfs(void* args){
  struct dbfs_input* inp = (struct dbfs_input*)args;
  // create a local copy of current vertex, so we can change it for input to next threads
  int current_vertex = inp->current_vertex;
  inp->visited[current_vertex] = 1;
  // create an array of tids to join them later (max n-1 new threads possible - other than this thread)
  pthread_t* tids = (pthread_t*)calloc(inp->number_of_nodes-1, sizeof(pthread_t));
  // index of the first unfilled tid
  int tidx = 0;
  int last_vertex_in_path = 1;
  for(int v = 0; v < inp->number_of_nodes; v++){
    if(!inp->visited[v] && inp->adj_matrix[current_vertex][v]){
      last_vertex_in_path = 0;
      inp->current_vertex = v;
      int x;
      if((x = pthread_create(&(tids[tidx++]), NULL, bfs, (void*)inp)) != 0){
        printf("Thread creation failed with error-bfs %d\n", x);
        pthread_exit((void*)(intptr_t)EXIT_FAILURE);
      }
    }
  }
  if(last_vertex_in_path){
    inp->output_array[(inp->output_index)++] = current_vertex;
  } else {
    // join all created threads
    for(int i = 0; i < tidx; i++){
      int x;
      if((x = pthread_join(tids[i], NULL)) != 0){
        printf("Thread join failed with error %d\n", x);
        pthread_exit((void*)(intptr_t)EXIT_FAILURE);
      }
    }
  }
  // free calloced memory
  free(tids);
  pthread_exit((void*)(intptr_t)EXIT_SUCCESS);
}

void* bfs(void* args){
  struct dbfs_input* inp = (struct dbfs_input*)args;
  // create a local copy of current vertex, so we can change it for input to next threads
  int current_vertex = inp->current_vertex;
  inp->visited[current_vertex] = 1;
  // create an array of tids to join them later (max n-1 new threads possible - other than this thread)
  pthread_t* tids = (pthread_t*)calloc(inp->number_of_nodes-1, sizeof(pthread_t));
  // index of the first unfilled tid
  int tidx = 0;
  int last_vertex_in_path = 1;
  for(int v = 0; v < inp->number_of_nodes; v++){
    if(!inp->visited[v] && inp->adj_matrix[current_vertex][v]){
      last_vertex_in_path = 0;
      inp->current_vertex = v;
      int x;
      if((x = pthread_create(&(tids[tidx++]), NULL, bfs, (void*)inp)) != 0){
        printf("Thread creation failed with error-bfs %d\n", x);
        pthread_exit((void*)(intptr_t)EXIT_FAILURE);
      }
    }
  }
  if(last_vertex_in_path){
    inp->output_array[(inp->output_index)++] = current_vertex;
  } else {
    // join all created threads
    for(int i = 0; i < tidx; i++){
      int x;
      if((x = pthread_join(tids[i], NULL)) != 0){
        printf("Thread join failed with error %d\n", x);
        pthread_exit((void*)(intptr_t)EXIT_FAILURE);
      }
    }
  }
  // free calloced memory
  free(tids);
  pthread_exit((void*)(intptr_t)EXIT_SUCCESS);
}

void *reader(void* args){
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
  printf("Thread received input>sno:%d:opno:%d:fname:%s:\n", sequence_number, operation_number, file_name);

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
  int starting_vertex = shmPtr[counter++];
  // detach the shmPtr, ! destorying shm is the work of client
  if(shmdt(shmPtr) == -1){
    perror("shmdt");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }
  // SYNC: Reader <Entry Section> Starts

  // <Entry Section> Ends
  // <CRITICAL SECTION> starts
  // open the file in read mode (it is assumed that it exists)
  FILE* file = fopen(file_name, "r");
  if(file == NULL){
    perror("fopen");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }

  // populate graph traversal input struct
  struct dbfs_input * graph_input = (struct dbfs_input*)calloc(1, sizeof(struct dbfs_input));
  graph_input->current_vertex = starting_vertex;
  fscanf(file, "%d", &(graph_input->number_of_nodes));
  graph_input->output_index = 0;
  // if the starting vertex comes in the ouput array, no other vertex will come and vice versa
  graph_input->output_array = (int *)calloc(graph_input->number_of_nodes - 1, sizeof(int));
  graph_input->visited = (int*)calloc(graph_input->number_of_nodes, sizeof(int));
  graph_input->adj_matrix = (int**)calloc(graph_input->number_of_nodes, sizeof(int*));
  for(int r = 0; r < graph_input->number_of_nodes; r++){
    graph_input->adj_matrix[r] = (int*)calloc(graph_input->number_of_nodes, sizeof(int));
    for(int c = 0; c < graph_input->number_of_nodes; c++)
      fscanf(file, "%d", &(graph_input->adj_matrix[r][c]));
  }

  // close the file - all reading done
  if(fclose(file) != 0){
    perror("fclose");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }

  // <CRITICAL SECTION> ends
  // SYNC: Reader <Exit Section>

  // <Exit Section> Ends

  // save thread tid for joining
  pthread_t tid;
  if(operation_number == 3){
    int x;
    if((x = pthread_create(&tid, NULL, dfs, (void*)graph_input)) != 0){
      printf("Thread creation failed with error-dfs %d\n", x);
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
  } else {
    int x;
    if((x = pthread_create(&tid, NULL, bfs, (void*)graph_input)) != 0){
      printf("Thread creation failed with error-bfs %d\n", x);
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
  }
  // join the thread
  int x;
  if((x = pthread_join(tid, NULL)) != 0){
    printf("Thread join failed with error %d\n", x);
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }

  // free the memory assigned for the struct
  free(graph_input->output_array);
  free(graph_input->visited);
  for(int v = 0; v < graph_input->number_of_nodes; v++)
    free(graph_input->adj_matrix[v]);
  free(graph_input->adj_matrix);
  free(graph_input);

  // create message (array as a string) and send to the client
  sprintf(buf.mtext, "%d", graph_input->output_array[0]);
  for(int i = 1; i <= graph_input->output_index; i++){
    sprintf(buf.mtext + 1 + 2*(i - 1), " %d", graph_input->output_array[i]);
  }
  buf.mtype  = sequence_number;
  if(msgsnd(msgqid, &buf, strlen(buf.mtext) + 1, 0) == -1){
    perror("msgsnd");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }
  return NULL;
}

int main(int argc, char const *argv[]){
  // getting the server number
  if(argc < 2){
    printf("Server number missing\n");
    exit(EXIT_FAILURE);
  } else if (atoi(argv[1]) != 1 && atoi(argv[1]) != 2){
    printf("Server number must be 1 or 2\n");
    exit(EXIT_FAILURE);
  }
  // assign secondary server number - 1 or 2
  int server_number = atoi(argv[1]);

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
    printf("\nSecondary Server %d Waiting for messages from Load Balancer...\n", server_number);
    // receive a message, truncate it if longer than buf.mtext's size
    if (msgrcv (msgqid, &buf, sizeof(buf.mtext), (server_number == 1)? SECONDARY_SERVER_1 : SECONDARY_SERVER_2, MSG_NOERROR) == -1){
      perror ("msgrcv");
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
    printf("Secondary Server %d Message received: %s\n", server_number,buf.mtext);

    // check if it is a termination message
    if (buf.mtext[0] == ')'){
      printf("Secondary Server %d Terminating...\n", server_number);
      break;
    }

    // allocate new space every time (to preserve other threads input) - free inside thread subroutine
    tin = (char*)calloc(20, sizeof(char));

    // set required values in tin
    strcpy(tin, buf.mtext);

    // create thread with thread id in tid (overwritten in tid), default attributes, write subroutine and tin as its input
    printf("Secondary Server %d Sending input to thread:%s\n", server_number, tin);
    if(pthread_create(&tid, NULL, reader, tin) != 0){
      perror("pthread_create");
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
  }
  // unlink named semaphores, destory mutexes

  pthread_exit((void*)(intptr_t)EXIT_SUCCESS);
}