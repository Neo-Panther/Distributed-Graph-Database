#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include "./utilities.h"
#include "./dfs_bfs.h"
#define MUTEX3 "/mutexthree"

/**Declaring global variables (shared between all threads)**/
// msg q vars
struct msgbuf buf;
int msgqid;
key_t key;

// variables for synchronization between threads
/*******************************************************/
sem_t* mutex3;  // name
sem_t* write_semaphore;
sem_t* read_semaphore;
sem_t* sync_shm_semaphore;

// sync shm ptr
int* syncShmPtr;

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
  // SYNC: Reader <Entry Section> Starts

  if(sem_wait(mutex3) == -1){
    perror("sem_wait-0");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }
  if(sem_wait(read_semaphore) == -1){
    perror("sem_wait-1");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }
  if(sem_wait(sync_shm_semaphore) == -1){
    perror("sem_wait-2");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }
  syncShmPtr[SHM_READ_COUNT]++;
  if(syncShmPtr[SHM_SEQUENCE_NUMBER] > sequence_number)
    syncShmPtr[SHM_SEQUENCE_NUMBER] = sequence_number;
  if(syncShmPtr[SHM_READ_COUNT] == 1){
    if(sem_post(sync_shm_semaphore) == -1){
      perror("sem_post-2");
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
    if(sem_wait(write_semaphore) == -1){
      perror("sem_wait-4");
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
  } else {
    if(sem_post(sync_shm_semaphore) == -1){
      perror("sem_post-2");
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
  }
  if(sem_post(read_semaphore) == -1){
    perror("sem_post-1");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }
  if(sem_post(mutex3) == -1){
    perror("sem_post-0");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }

  // <Entry Section> Ends
  printf("Entry->Critical\n");
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
  // SYNC: Reader <Exit Section> Starts

  if(sem_wait(sync_shm_semaphore) == -1){
    perror("sem_wait-3");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }
  syncShmPtr[SHM_READ_COUNT]--;
  if(syncShmPtr[SHM_SEQUENCE_NUMBER] == sequence_number)
    syncShmPtr[SHM_SEQUENCE_NUMBER] = 101;
  if(syncShmPtr[SHM_READ_COUNT] == 0){
    if(sem_post(write_semaphore) == -1){
      perror("sem_post-5");
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
  }
  if(sem_post(sync_shm_semaphore) == -1){
    perror("sem_post-3");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }

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
  // thread inp variable (pointer)
  char *tin;
  // create an array of tids to join them later (assume sequence number max till 100 => max 100 threads)
  pthread_t tids[100];
  // index of the first unfilled tid
  int tidx = 0;
  /*****************************************/

  /******Initializing Msg Q Variables*******/
  key = getMsgQKey();
  if ((msgqid = msgget(key, PERMS)) == -1){
    perror("msgget");
    exit(EXIT_FAILURE);
  }
  /*****************************************/

  /******Initializing Semaphores and Mutexes*******/
  if((write_semaphore = sem_open(WRITE_SEMAPHORE, O_CREAT, PERMS, 1)) == SEM_FAILED){
    perror("sem_open-write");
    exit(1);
  }
  if((read_semaphore = sem_open(READ_SEMAPHORE, O_CREAT, PERMS, 1)) == SEM_FAILED){
    perror("sem_open-read");
    exit(1);
  }
  // mutex attr is non-portable, thus we always use defaults. This function does not fail
  if((sync_shm_semaphore = sem_open(SYNC_SHM_SEMAPHORE, O_CREAT, PERMS, 1)) == SEM_FAILED){
    perror("sem_open-read");
    exit(1);
  }
  if((mutex3 = sem_open(MUTEX3, O_CREAT, PERMS, 1)) == SEM_FAILED){
    perror("sem_open-read");
    exit(1);
  }
  pthread_mutex_init(&queue_tail_mutex, NULL);
  pthread_mutex_init(&output_mutex, NULL);
  /*****************************************/

  // get sync semaphore
  // getting the sync shm
  int shmid = shmget(SYNC_SHM_KEY, SYNC_SHM_SIZE, PERMS);
  if(shmid == -1){
    perror("shmget-sync");
    exit(EXIT_FAILURE);
  }
  if((syncShmPtr = shmat(shmid,NULL,0)) == (void*)(intptr_t)-1){
    perror("shmat-sync");
    exit(EXIT_FAILURE);
  }

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
      while(tidx > 0){
        int x;
        if((x = pthread_join(tids[--tidx], NULL)) != 0){
          printf("Thread join failed with error-bfs %d\n", x);
          pthread_exit((void*)(intptr_t)EXIT_FAILURE);
        }
      }
      break;
    }

    // allocate new space every time (to preserve other threads input) - free inside thread subroutine
    tin = (char*)calloc(20, sizeof(char));

    // set required values in tin
    strcpy(tin, buf.mtext);

    // create thread with thread id in tid (overwritten in tid), default attributes, write subroutine and tin as its input
    printf("Secondary Server %d Sending input to thread:%s\n", server_number, tin);
    if(pthread_create(&(tids[tidx++]), NULL, reader, tin) != 0){
      perror("pthread_create");
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
  }
  // unlink named semaphores, destory mutexes
  sem_unlink(READ_SEMAPHORE);
  sem_unlink(WRITE_SEMAPHORE);
  sem_unlink(SYNC_SHM_SEMAPHORE);
  sem_unlink(MUTEX3);
  pthread_mutex_destroy(&queue_tail_mutex);
  pthread_mutex_destroy(&output_mutex);
  // waits for all running threads before exiting CHECK
  pthread_exit((void*)(intptr_t)EXIT_SUCCESS);
}