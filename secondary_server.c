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
pthread_mutex_t output_mutex;
pthread_mutex_t queue_tail_mutex;
// struct to send input for graph traversal
struct common_input {
  int number_of_nodes;
  int** adj_matrix;
  int* output_array;
  int* visited;
  int output_index;  // TODO: need protection
};

struct common_input_bfs {
  int number_of_nodes;
  int** adj_matrix;
  int* output_array;
  int* visited;
  int* queue;
  int queue_tail;  // TODO: need protection
  int output_index;  // TODO: need protection
};

// struct to send input to bfs nodes (bfs_helper) for level order traversal
struct bfs_input{
  int current_vertex;  // need to be reassigned for each thread
  struct common_input_bfs* common;
};

struct dfs_input{
  int current_vertex;  // need to be reassigned for each thread
  struct common_input* common;
};

// agrs of type dfs input
void* dfs(void* args){
  struct dfs_input* inp = (struct dfs_input*)args;
  // create a local copy of current vertex
  int current_vertex = inp->current_vertex;
  struct common_input* common = inp->common;
  free(inp);
  common->visited[current_vertex] = 1;
  printf("DFS Thread:%lu:Current vertex b4 calls<%d> visited::", pthread_self(),current_vertex);
  for(int i = 0; i < common->number_of_nodes; i++)
    printf("%d ", common->visited[i]);
    printf("\n");
  // create an array of tids to join them later (max n-1 new threads possible - other than this thread)
  pthread_t* tids = (pthread_t*)calloc(common->number_of_nodes-1, sizeof(pthread_t));
  // index of the first unfilled tid
  int tidx = 0;
  for(int v = 0; v < common->number_of_nodes; v++){
    if(!common->visited[v] && common->adj_matrix[current_vertex][v]){
      printf("DFS Thread:%lu: going from CV:%d: to:%d:\n", pthread_self(), current_vertex, v);
      inp = (struct dfs_input*)calloc(1, sizeof(struct dfs_input));
      inp->current_vertex = v;
      inp->common = common;
      int x;
      if((x = pthread_create(&(tids[tidx++]), NULL, dfs, (void*)inp)) != 0){
        printf("Thread creation failed with error-dfs %d\n", x);
        pthread_exit((void*)(intptr_t)EXIT_FAILURE);
      }
    }
  }
  printf("DFS Thread:%lu:Current vertex<%d> aft calls\n", pthread_self(), current_vertex);
  if(!tidx){
    printf("DFS Thread:%lu:Adding CV:%d at output_index:%d\n", pthread_self(),current_vertex, common->output_index);
    // this is the last vertex on a path
    pthread_mutex_lock(&output_mutex);
    common->output_array[(common->output_index)++] = current_vertex;
    pthread_mutex_unlock(&output_mutex);
  }
  // join all created threads
  while(tidx > 0){
    int x;
    if((x = pthread_join(tids[--tidx], NULL)) != 0){
      printf("Thread join failed with error-dfs %d\n", x);
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
  }
  printf("DFS Thread:%lu: ending\n", pthread_self());
  // free calloced memory
  free(tids);
  pthread_exit((void*)(intptr_t)EXIT_SUCCESS);
}

void* bfs_helper(void* args){
  // extract input info and free the address
  struct bfs_input* inp = (struct bfs_input*)args;
  struct common_input_bfs* common = inp->common;
  int current_vertex = inp->current_vertex;
  printf("BFS Helper Thread:%lu:Start CV:%d\n", pthread_self(), current_vertex);
  free(inp);
  printf("llk\n");
  for(int v = 0; v < common->number_of_nodes; v++){
    if(!common->visited[v] && common->adj_matrix[current_vertex][v]){
      printf("BFS Helper Thread:%lu:add v to q:%d\n", pthread_self(), v);
      pthread_mutex_lock(&queue_tail_mutex);
      common->queue[(common->queue_tail)++] = v;
      pthread_mutex_unlock(&queue_tail_mutex);
      common->visited[v] = 1;
    }
  }
  printf("knf\n");
  return (void*)(intptr_t)EXIT_SUCCESS;
}

void* bfs(void* args){
  struct dfs_input* inp = (struct dfs_input*)args;
  int current_vertex = inp->current_vertex;
  struct common_input* common = inp->common;
  free(inp);
  common->visited[current_vertex] = 1;
  printf("BFS Main Thread:%lu:Current vertex b4 calls<%d> visited::", pthread_self(),current_vertex);
  for(int i = 0; i < common->number_of_nodes; i++)
    printf("%d ", common->visited[i]);
    printf("\n");
  // create an array of tids to join them later (max n-1 new threads possible - other than this thread)
  pthread_t* tids = (pthread_t*)calloc(common->number_of_nodes-1, sizeof(pthread_t));
  // index of the first unfilled tid
  int tidx = 0;

  // populate the helper's struct
  struct common_input_bfs* common_bfs = (struct common_input_bfs*)calloc(1, sizeof(struct common_input_bfs));
  common_bfs->queue_tail = 1;
  int queue_head = 0;
  common_bfs->queue = (int*)calloc(common->number_of_nodes, sizeof(int));
  common_bfs->queue[0] = current_vertex;
  common_bfs->adj_matrix = common->adj_matrix;
  common_bfs->number_of_nodes = common->number_of_nodes;
  common_bfs->output_array = common->output_array;
  common_bfs->output_index = common->output_index;
  common_bfs->visited = common->visited;

  // go to a new level sequentially
  while(queue_head < common_bfs->queue_tail){
    printf("BFS Thread Main:%lu:queue_head:%d:tail:%d:visited::", pthread_self(), queue_head, common_bfs->queue_tail);
    for(int i = 0; i < common->number_of_nodes; i++)
      printf("%d ", common->visited[i]);
    printf("\n");
    printf("BFS Thread Main:%lu:q::", pthread_self());
    for(int i = queue_head; i < common_bfs->queue_tail; i++)
      printf("%d ", common_bfs->queue[i]);
    printf("\n");
    int end = common_bfs->queue_tail;

    struct bfs_input* helper_input;

    // start new thread for each node at this level
    while(queue_head < end){
      int x;
      common_bfs->output_array[(common->output_index)++] = common_bfs->queue[queue_head];
      helper_input = (struct bfs_input*)calloc(1, sizeof(struct bfs_input));
      helper_input->common = common_bfs;
      helper_input->current_vertex = common_bfs->queue[queue_head++];
      if((x = pthread_create(&(tids[tidx++]), NULL, bfs_helper, (void*)helper_input)) != 0){
        printf("Thread creation failed with error-dfs %d\n", x);
        pthread_exit((void*)(intptr_t)EXIT_FAILURE);
      }
    }
    // join all threads created above (complete the concurrent traversal of a level)
    while(tidx > 0){
      int x;
      if((x = pthread_join(tids[--tidx], NULL)) != 0){
        printf("Thread join failed with error-bfs %d\n", x);
        pthread_exit((void*)(intptr_t)EXIT_FAILURE);
      }
    }
  }
  // free calloced memory
  free(common_bfs->queue);
  free(common_bfs);
  free(tids);
  // current vertex freed in helper
  // visited and adj_matrix will be freed outside
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
  printf("Reader Thread received input:sno:%d:opno:%d:fname:%s:\n", sequence_number, operation_number, file_name);

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
  printf("Reader Thread waiting for client to finish writing...\n");
  while(shmPtr[counter] != 1);
  counter++;
  int starting_vertex = shmPtr[counter++];
  // detach the shmPtr, ! destorying shm is the work of client
  if(shmdt(shmPtr) == -1){
    perror("shmdt");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }
  printf("Reader received starting vertex: %d\n", starting_vertex);
  // TODO: SYNC: Reader <Entry Section> Starts

  // <Entry Section> Ends
  // <CRITICAL SECTION> starts
  // open the file in read mode (it is assumed that it exists)
  FILE* file = fopen(file_name, "r");
  if(file == NULL){
    perror("fopen");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }

  // populate graph traversal input struct
  struct common_input * graph_input = (struct common_input*)calloc(1, sizeof(struct common_input));
  fscanf(file, "%d", &(graph_input->number_of_nodes));
  graph_input->output_index = 0;
  // max n vertices in output (of bfs)
  graph_input->output_array = (int *)calloc(graph_input->number_of_nodes, sizeof(int));
  graph_input->visited = (int*)calloc(graph_input->number_of_nodes, sizeof(int));
  graph_input->adj_matrix = (int**)calloc(graph_input->number_of_nodes, sizeof(int*));
  for(int r = 0; r < graph_input->number_of_nodes; r++){
    graph_input->adj_matrix[r] = (int*)calloc(graph_input->number_of_nodes, sizeof(int));
    for(int c = 0; c < graph_input->number_of_nodes; c++)
      fscanf(file, "%d", &(graph_input->adj_matrix[r][c]));
  }
  printf("Reader sending bdfs input:starting_index:%d:output_index:%d:\
  number_of_nodes:%d\n", starting_vertex-1, graph_input->output_index, graph_input->number_of_nodes);

  printf(":Read Adj matrix:\n");
  for(int i = 0; i < graph_input->number_of_nodes; i++){
    for(int j = 0; j < graph_input->number_of_nodes; j++){
      printf("%d ", graph_input->adj_matrix[i][j]);
    }
    printf("\n");
  }

  // close the file - all reading done
  if(fclose(file) != 0){
    perror("fclose");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }

  // <CRITICAL SECTION> ends
  // TODO: SYNC: Reader <Exit Section> Starts

  // <Exit Section> Ends
  // save thread tid for joining
  pthread_t tid;
  if(operation_number == 3){
    int x;
    struct dfs_input* inp = (struct dfs_input*)calloc(1, sizeof(struct dfs_input));
    inp->common = graph_input;
    inp->current_vertex = starting_vertex - 1;
    if((x = pthread_create(&tid, NULL, dfs, (void*)inp)) != 0){
      printf("Thread creation failed with error-dfs %d\n", x);
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
    printf("dfs thread active\n");
  } else {
    int x;
    struct dfs_input* inp = (struct dfs_input*)calloc(1, sizeof(struct dfs_input));
    inp->common = graph_input;
    inp->current_vertex = starting_vertex - 1;
    if((x = pthread_create(&tid, NULL, bfs, (void*)inp)) != 0){
      printf("Thread creation failed with error-bfs %d\n", x);
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
    printf("bfs thread active\n");
  }
  // join the thread
  int x;
  if((x = pthread_join(tid, NULL)) != 0){
    printf("Thread join failed with error %d\n", x);
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }
  printf("done3\n");

  // create message (array as a string) and send to the client
  printf("Reader received output:index:%d\n", graph_input->output_index);
  for(int i = 0; i < graph_input->output_index; i++)
    printf("%d ", graph_input->output_array[i]);
  printf("\n");
  sprintf(buf.mtext, "%d", graph_input->output_array[0] + 1);
  for(int i = 1; i < graph_input->output_index; i++){
    sprintf(buf.mtext + 1 + 2*(i - 1), " %d", graph_input->output_array[i] + 1);
  }
  buf.mtype  = sequence_number;
  if(msgsnd(msgqid, &buf, strlen(buf.mtext) + 1, 0) == -1){
    perror("msgsnd");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }
  // free the memory assigned for the struct
  free(graph_input->output_array);
  free(graph_input->visited);
  for(int v = 0; v < graph_input->number_of_nodes; v++)
    free(graph_input->adj_matrix[v]);
  free(graph_input->adj_matrix);
  free(graph_input);
  pthread_exit((void*)(intptr_t)EXIT_SUCCESS);
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
  pthread_mutex_init(&queue_tail_mutex, NULL);
  pthread_mutex_init(&output_mutex, NULL);
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
  // TODO: unlink named semaphores, destory mutexes
  pthread_mutex_destroy(&queue_tail_mutex);
  pthread_mutex_destroy(&output_mutex);
  pthread_exit((void*)(intptr_t)EXIT_SUCCESS);
}