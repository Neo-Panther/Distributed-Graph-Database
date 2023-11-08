# OS Assignment 2

## TODOs Left:
Extensive testing and confirmation of the reader-writer solution

## Common Doubts:
1. What should be the return values on (exit with error) across all files.
2. Can we use sequence number as the unique key for shared memory spaces, and message queue mtype for client-thread communication?: **Yes**: It is written in client section that this number is unique across client requests.
3. Should we check for existence of file for operation 1 and non-existence for option 2?: **NO**: It is mentioned in client section that the user will not deliberately do this (deliberately implies our execution order may create a condition when this happens- we must make sure that it doesn't happen).

*NOTE*: We will solve the reader writer problem by giving priority to writers (no new reader may access the graph DB if a writer has requested access to it, until it is done), solve reader starvation problem by allowing readers of lower sequence numbers than writers.

*NOTE2*: Use pthread_exit in main of multi threaded processes to wait for all threads to complete before exiting main.

*NOTE3*: mutex3 ensures that if both reader(s) and writer(s) are waiting for release of reader semaphore, writer(s) enter first, since reader(s) also have to wait for mutex3 befor reader semaphore.