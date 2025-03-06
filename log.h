#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include "banking.h"
#include "pa2345.h"

void log_started(FILE *events_log_file, local_id process_id, balance_t balance);
void log_received_all_started(FILE *events_log_file, local_id process_id);
void log_done(FILE *events_log_file, local_id process_id, balance_t balance);
void log_received_all_done(FILE *events_log_file, local_id process_id);
void log_pipe(FILE *pipes_log_file, local_id from, local_id to, int read, int write);
void log_transfer_out(FILE *events_log_file, TransferOrder *trnsfr);
void log_transfer_in(FILE *events_log_file, TransferOrder *trnsfr);

#endif // LOG_H
