#include "pipe_utils.h"
#include "common.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

Pipeline* create_pipeline(int process_count) {
    Pipeline *p = malloc(sizeof(Pipeline));
    p->size_value = process_count;
    p->size = &(p->size_value);
    FILE *pipes_log_file = fopen(pipes_log, "w");

    for (int i = 0; i < process_count; i++) {
        for (int j = 0; j < process_count; j++) {
            if (i == j) {
                p->pipes[i][j][0] = -1;
                p->pipes[i][j][1] = -1;
                continue;
            }
            if (pipe(p->pipes[i][j]) < 0) {
                perror("pipe");
            }
            if (fcntl(p->pipes[i][j][0], F_SETFL, O_NONBLOCK) < 0 || fcntl(p->pipes[i][j][1], F_SETFL, O_NONBLOCK) < 0) {
                perror("fcntl");
            }
            log_pipe(pipes_log_file, i, j, p->pipes[i][j][0], p->pipes[i][j][1]);
        }
    }
    fclose(pipes_log_file);
    return p;
}

void close_unused_pipes(Pipeline *p, local_id self) {
    int total = p->size_value;
    for (int i = 0; i < total; i++) {
        for (int j = 0; j < total; j++) {
            if (i == j) continue;
            if (self == i) {
                close(p->pipes[i][j][0]);
            } else if (self == j) {
                close(p->pipes[i][j][1]);
            } else {
                close(p->pipes[i][j][0]);
                close(p->pipes[i][j][1]);
            }
        }
    }
}

int getReaderById(local_id from, local_id to, Pipeline *p) {
    return p->pipes[from][to][0];
}

int getWriterById(local_id from, local_id to, Pipeline *p) {
    return p->pipes[from][to][1];
}
