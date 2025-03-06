#include "ipc.h"


typedef struct Process {
    local_id id;
    int total_processes;
    void* pipeline; 
} Process;

typedef Process* ProcessPtr;

local_id getSelfId(ProcessPtr p);
void* getPipeline(ProcessPtr p);
ProcessPtr createProcess(local_id id, int total_processes, void* pipeline);

