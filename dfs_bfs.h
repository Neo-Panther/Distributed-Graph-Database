#ifndef DFSBFS_H
#define DFSBFS_H
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include "./utilities.h"

pthread_mutex_t output_mutex;
pthread_mutex_t queue_tail_mutex;
// struct to send input for graph traversal
struct common_input {
  int number_of_nodes;
  int** adj_matrix;
  int* output_array;
  int* visited;
  int output_index;
};

struct common_input_bfs {
  int number_of_nodes;
  int** adj_matrix;
  int* visited;
  int* queue;
  int queue_tail;
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
  for(int v = 0; v < common->number_of_nodes; v++){
    if(!common->visited[v] && common->adj_matrix[current_vertex][v]){
      printf("BFS Helper Thread:%lu:add v to q:%d\n", pthread_self(), v);
      pthread_mutex_lock(&queue_tail_mutex);
      common->queue[(common->queue_tail)++] = v;
      pthread_mutex_unlock(&queue_tail_mutex);
      common->visited[v] = 1;
    }
  }
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

  // populate the shared items
  struct common_input_bfs* common_bfs = (struct common_input_bfs*)calloc(1, sizeof(struct common_input_bfs));
  common_bfs->queue_tail = 1;
  int queue_head = 0;
  common_bfs->queue = (int*)calloc(common->number_of_nodes, sizeof(int));
  common_bfs->queue[0] = current_vertex;
  common_bfs->adj_matrix = common->adj_matrix;
  common_bfs->number_of_nodes = common->number_of_nodes;
  common_bfs->visited = common->visited;
  common->output_array[(common->output_index)++] = current_vertex;

  // go to a new level sequentially
  while(queue_head < common_bfs->queue_tail){
    int end = common_bfs->queue_tail;
    struct bfs_input* helper_input;

    // start new thread for each node at this level
    while(queue_head < end){
      int x;
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
    // add this level to the output array
    for(int i = queue_head; i < common_bfs->queue_tail; i++)
      common->output_array[(common->output_index)++] = common_bfs->queue[i];
  }
  // free calloced memory
  free(common_bfs->queue);
  free(common_bfs);
  free(tids);
  // current vertex freed in helper
  // visited and adj_matrix will be freed outside
  pthread_exit((void*)(intptr_t)EXIT_SUCCESS);
}
#endif