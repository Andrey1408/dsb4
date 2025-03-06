#include "process_factory.h"
#include <stdlib.h>

local_id getSelfId(ProcessPtr p) {
    return p->id;
}

void* getPipeline(ProcessPtr p) {
    return p->pipeline;
}

ProcessPtr createProcess(local_id id, int total_processes, void* pipeline) {
    ProcessPtr p = malloc(sizeof(struct Process));
    if (!p) return NULL;
    p->id = id;
    p->total_processes = total_processes;
    p->pipeline = pipeline;
    return p;
}
