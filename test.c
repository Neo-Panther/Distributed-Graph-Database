#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
#include "./utilities.h"
#define MUTEX3 "/mutexthree"

// variables for synchronization between threads
pthread_mutex_t mutex2;
unsigned int writer_count;

sem_t* seq_semaphore;
sem_t* mutex3;
sem_t* write_semaphore;
sem_t* read_semaphore;
sem_t* sync_shm_semaphore;

// sync shm ptr
int* syncShmPtr;

void* reader(void* args){
  // SYNC: Reader <Entry Section> Starts
  int tid = (int)(intptr_t)args;
  int sequence_number = (int)(intptr_t)args;
  printf("%d reader Entering entry section\n", tid); fflush(stdout);

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
  if(syncShmPtr[SHM_SEQUENCE_NUMBER] > sequence_number){
    syncShmPtr[SHM_SEQUENCE_NUMBER] = sequence_number;
    if(sem_wait(seq_semaphore) == -1){
      perror("sem_wait-2.5");
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
  }
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
  printf("%d reader Entry->Critical\n", tid); fflush(stdout);
  // <CRITICAL SECTION> starts
  
  for(int i = 0; i < 100000; i++);

  // <CRITICAL SECTION> ends
  printf("%d reader Critical->Exit\n", tid); fflush(stdout);
  // SYNC: Reader <Exit Section> Starts

  if(sem_wait(sync_shm_semaphore) == -1){
    perror("sem_wait-3");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }
  syncShmPtr[SHM_READ_COUNT]--;
  if(syncShmPtr[SHM_SEQUENCE_NUMBER] == sequence_number){
    syncShmPtr[SHM_SEQUENCE_NUMBER] = INT16_MAX;  // assuming sequence number never exceeds this (<= 100 given)
    if(sem_post(seq_semaphore) == -1){
      perror("sem_post-2.5");
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
  }
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
  printf("%d reader done\n", tid); fflush(stdout);
}

void *writer(void* args){
  // SYNC: Writer <Entry Section> Starts
  int tid = (int)(intptr_t)args;
  int sequence_number = (int)(intptr_t)args;
  printf("%d writer Entering entry section\n", tid); fflush(stdout);
  
  if(sem_wait(sync_shm_semaphore) == -1){
    perror("sem_wait-0");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }
  // wait if there are earlier writers left to read
  if(syncShmPtr[SHM_SEQUENCE_NUMBER] < sequence_number){
    if(sem_post(sync_shm_semaphore) == -1){
      perror("sem_post-0");
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
    printf("%d writer now waiting for earlier readers\n", tid); fflush(stdout);
    if(sem_wait(seq_semaphore) == -1){
      perror("sem_wait-0.5");
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
    if(sem_post(seq_semaphore) == -1){
      perror("sem_post-0.5");
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
    if(sem_wait(sync_shm_semaphore) == -1){
      perror("sem_wait-0");
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
  }
  if(sem_post(sync_shm_semaphore) == -1){
    perror("sem_post-0");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }
  if(pthread_mutex_lock(&mutex2) != 0){
    printf("Mutex not initialized properly-1\n");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }

  writer_count += 1;
  if(writer_count == 1){
    if(sem_wait(read_semaphore) == -1){
      perror("sem_wait-1");
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
  }
  pthread_mutex_unlock(&mutex2);
  if(sem_wait(write_semaphore) == -1){
    perror("sem_wait-2");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }

  // <Entry Section> Ends
  printf("%d writer Entry->Critical\n", tid); fflush(stdout);

  // <CRITICAL SECTION> starts

  for(int i = 0; i < 100000; i++);

  // <CRITICAL SECTION> ends
  printf("%d writer Critical->Exit\n", tid); fflush(stdout);

  // SYNC: Writer <Exit Section>

  if(sem_post(write_semaphore) == -1){
    perror("sem_post-1");
    pthread_exit((void*)(intptr_t)EXIT_FAILURE);
  }
  pthread_mutex_lock(&mutex2);
  writer_count -= 1;
  if(writer_count == 0){
    if(sem_post(read_semaphore) == -1){
      perror("sem_post-2");
      pthread_exit((void*)(intptr_t)EXIT_FAILURE);
    }
  }
  pthread_mutex_unlock(&mutex2);

  // <Exit Section> Ends
  printf("%d writer done\n", tid); fflush(stdout);
}

int main(){
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
  if((seq_semaphore = sem_open(SEQ_NUM_SEMAPHORE, O_CREAT, PERMS, 1)) == SEM_FAILED){
    perror("sem_open-seq");
    exit(1);
  }
  pthread_mutex_init(&mutex2, NULL);
  /*****************************************/

  // get sync semaphore
  // getting the sync shm
  // create process sync shm
  int shmid=shmget(SYNC_SHM_KEY, SYNC_SHM_SIZE, PERMS|IPC_CREAT);
  if(shmid==-1){
    perror("shmget-sync");
    exit(EXIT_FAILURE);
  }
  syncShmPtr = shmat(shmid,NULL,0);
  if(syncShmPtr == (void*)(intptr_t)-1){
    perror("shmat-sync");
    exit(EXIT_FAILURE);
  }

  // initialize sync shm
  syncShmPtr[SHM_READ_COUNT] = 0;
  syncShmPtr[SHM_SEQUENCE_NUMBER] = INT16_MAX;

  // call threads
  pthread_t tid;
  pthread_create(&tid, NULL, reader, (void*)(intptr_t)1);
  pthread_create(&tid, NULL, reader, (void*)(intptr_t)2);
  pthread_create(&tid, NULL, writer, (void*)(intptr_t)3);
  pthread_create(&tid, NULL, writer, (void*)(intptr_t)4);
  pthread_create(&tid, NULL, reader, (void*)(intptr_t)5);
  pthread_create(&tid, NULL, writer, (void*)(intptr_t)6);
  pthread_create(&tid, NULL, writer, (void*)(intptr_t)7);
  pthread_create(&tid, NULL, writer, (void*)(intptr_t)8);

  pthread_join(tid, NULL);

  // detach sync shm - destroyed in load balancer
  if(shmdt(syncShmPtr)== -1){
    perror("shmdt-sync");
    exit(EXIT_FAILURE);
  }
  // delete sync shm
  if(shmctl(shmid, IPC_RMID, 0) == -1){
    perror("shmctl");
    exit(EXIT_FAILURE);
  }
  // unlink named semaphores, destory mutexes
  sem_unlink(READ_SEMAPHORE);
  sem_unlink(SEQ_NUM_SEMAPHORE);
  sem_unlink(WRITE_SEMAPHORE);
  sem_unlink(SYNC_SHM_SEMAPHORE);
  sem_unlink(MUTEX3);
  pthread_mutex_destroy(&mutex2);
  // waits for all running threads before exiting CHECK
  pthread_exit((void*)(intptr_t)EXIT_SUCCESS);
}