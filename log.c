#include "log.h"
#include "banking.h"
#include <stdio.h>
#include <unistd.h>


static const char *const log_pipe_open_fmt = "Pipe from process %d to %d was opened | read: %d, write: %d\n";

void log_started(FILE *events_log_file, local_id process_id, balance_t balance) {
    timestamp_t time = get_lamport_time();  
    
    if (events_log_file) {
        fprintf(events_log_file, log_started_fmt, time, process_id, getpid(), getppid(), balance);
        fflush(events_log_file);
    }
    fflush(stdout);
}

void log_received_all_started(FILE *events_log_file, local_id process_id) {
    timestamp_t time = get_lamport_time();  
    
    if (events_log_file) {
        fprintf(events_log_file, log_received_all_started_fmt, time, process_id);
        fflush(events_log_file);
    }
    fflush(stdout);
}

void log_done(FILE *events_log_file, local_id process_id, balance_t balance) {
    timestamp_t time = get_lamport_time();  
    
    if (events_log_file) {
        fprintf(events_log_file, log_done_fmt, time, process_id, balance);
        fflush(events_log_file);
    }
    fflush(stdout);
}

void log_received_all_done(FILE *events_log_file, local_id process_id) {
    timestamp_t time = get_lamport_time();  
    
    if (events_log_file) {
        fprintf(events_log_file, log_received_all_done_fmt, time, process_id);
        fflush(events_log_file);
    }
    fflush(stdout);
}

void log_pipe(FILE *pipes_log_file, local_id from, local_id to, int read, int write) {
    if (pipes_log_file) {
        fprintf(pipes_log_file, log_pipe_open_fmt, from, to, read, write);
        fflush(pipes_log_file);
    }
}

void log_transfer_out(FILE *events_log_file, TransferOrder *trnsfr) {
    timestamp_t time = get_lamport_time();  
    
    if (events_log_file) {
        fprintf(events_log_file, log_transfer_out_fmt, time, trnsfr->s_src, trnsfr->s_amount, trnsfr->s_dst);
        fflush(events_log_file);
    }
    fflush(stdout);
}

void log_transfer_in(FILE *events_log_file, TransferOrder *trnsfr) {
    timestamp_t time = get_lamport_time();  
    
    if (events_log_file) {
        fprintf(events_log_file, log_transfer_in_fmt, time, trnsfr->s_dst, trnsfr->s_amount, trnsfr->s_src);
        fflush(events_log_file);
    }
    fflush(stdout);
}
