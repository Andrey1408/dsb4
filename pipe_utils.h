#include "ipc.h"


typedef struct Pipeline {
    int pipes[MAX_PROCESS_ID+1][MAX_PROCESS_ID+1][2];
    int size_value;
    int *size; 
} Pipeline;

Pipeline* create_pipeline(int process_count);
void close_unused_pipes(Pipeline *p, local_id self);

int getReaderById(local_id from, local_id to, Pipeline *p);
int getWriterById(local_id from, local_id to, Pipeline *p);

