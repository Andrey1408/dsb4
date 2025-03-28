#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include "common.h"
#include "ipc.h"
#include "pipe_utils.h"
#include "process_factory.h"
#include "banking.h"
#include "pa2345.h"
#include "log.h"

FILE *global_events_log_file = NULL;
timestamp_t lamport_time = 0; // Глобальное время Лампорта
timestamp_t get_lamport_time(void)
{
    return lamport_time;
}
void handle_message(ProcessPtr p, Message *msg);
void add_to_queue(ProcessPtr p, local_id id, timestamp_t time);
void remove_from_queue(local_id id);
int is_highest_priority(ProcessPtr p);
int all_replies_received(ProcessPtr p);
void send_cs_request(ProcessPtr p);
void send_cs_reply(ProcessPtr p, local_id to);
void send_cs_release(ProcessPtr p);
void log_queue(ProcessPtr p);
void request_handler(ProcessPtr p, Message *msg, local_id *sender);
int started_handler(ProcessPtr p, local_id *sender);
int done_handler(ProcessPtr p, local_id *sender);

typedef struct
{
    local_id process_id;
    timestamp_t timestamp;
} Request;

Request queue[MAX_PROCESS_ID + 1];
timestamp_t queue_enter_time = -1;
int queue_size = 0;
int replies_received[MAX_PROCESS_ID + 1] = {0};
int release_received[MAX_PROCESS_ID + 1] = {0};
int started_received[MAX_PROCESS_ID + 1] = {0};
int done_received[MAX_PROCESS_ID + 1] = {0};
int all_started_received = 0;
int all_done_received = 0;

void handle_message(ProcessPtr p, Message *msg)
{
    local_id sender;
    if (msg->s_header.s_payload_len >= sizeof(local_id))
    {
        memcpy(&sender, msg->s_payload, sizeof(local_id));
    }
    else
    {
        fprintf(stderr, "Process %d: Received invalid message with payload length %d\n", p->id, msg->s_header.s_payload_len);
        return;
    }
    fprintf(stderr, "Process %d: Received message type %d from process %d at Lamport time %d with time attached %d\n",
            p->id, msg->s_header.s_type, sender, lamport_time, msg->s_header.s_local_time);
    switch (msg->s_header.s_type)
    {
    case STARTED:
        if (started_handler(p, &sender))
        {
            all_started_received = 1;
        }
        break;
    case DONE:
        if (done_handler(p, &sender))
        {
            all_done_received = 1;
        }
        break;
    case CS_REQUEST:
        request_handler(p, msg, &sender);
        break;
    case CS_REPLY:
        replies_received[sender] = 1;
        break;
    case CS_RELEASE:
        remove_from_queue(sender);
        replies_received[sender] = 1;
        release_received[sender] = 1;
        break;
    default:
        fprintf(stderr, "Process %d: Unhandled message read\n", p->id);
    }
}

int started_handler(ProcessPtr p, local_id *sender)
{
    started_received[*sender] = 1;
    for (local_id i = 1; i < p->total_processes; i++)
    {
        if (i != p->id && started_received[i] == 0)
            return 0;
    }
    return 1;
}

int done_handler(ProcessPtr p, local_id *sender)
{
    done_received[*sender] = 1;
    for (local_id i = 1; i < p->total_processes; i++)
    {
        if (i != p->id && done_received[i] == 0)
            return 0;
    }
    return 1;
}

void request_handler(ProcessPtr p, Message *msg, local_id *sender)
{
    if (msg->s_header.s_local_time >= queue_enter_time)
    {
        add_to_queue(p, *sender, msg->s_header.s_local_time - 1);
    }
    else
    {
        send_cs_reply(p, *sender);
    }
}

void log_queue(ProcessPtr p)
{
    fprintf(stderr, "Process %d: Queue state: ", p->id);
    for (int i = 0; i < queue_size; i++)
    {
        fprintf(stderr, "(%d, %d) ", queue[i].process_id, queue[i].timestamp);
    }
    fprintf(stderr, "\n");
    fprintf(global_events_log_file, "\n");
}

void sort_queue(ProcessPtr p)
{
    for (int pass = 0; pass < queue_size; pass++)
    {
        for (int i = queue_size - 1; i > 0; i--)
        {
            if (queue[i].timestamp < queue[i - 1].timestamp ||
                (queue[i].timestamp == queue[i - 1].timestamp && queue[i].process_id < queue[i - 1].process_id))
            {
                Request temp = queue[i];
                queue[i] = queue[i - 1];
                queue[i - 1] = temp;
            }
        }
    }
    log_queue(p);
}

void add_to_queue(ProcessPtr p, local_id id, timestamp_t time)
{
    for (int i = 0; i < queue_size; i++)
    {
        if (queue[i].process_id == id)
            return;
    }

    if (queue_size >= MAX_PROCESS_ID + 1)
    {
        fprintf(stderr, "Process %d: Queue overflow!\n", p->id);
        return;
    }
    queue[queue_size].process_id = id;
    queue[queue_size].timestamp = time;
    queue_size++;
    // Сортировка очереди
    sort_queue(p);
}

void remove_from_queue(local_id id)
{
    for (int i = 0; i < queue_size; i++)
    {
        if (queue[i].process_id == id)
        {
            for (int j = i; j < queue_size - 1; j++)
                queue[j] = queue[j + 1];
            queue_size--;
            break;
        }
    }
}

int is_highest_priority(ProcessPtr p)
{
    if (queue_size == 0 || queue[0].process_id != p->id)
        return 0;
    return 1;
}

int all_release_received(ProcessPtr p)
{
    for (local_id i = 1; i < p->total_processes; i++)
    {
        if (i != p->id && release_received[i] == 0)
        {
            fprintf(stderr, "Process %d: in release received: %d\n", p->id, 0);
            return 0;
        }
    }
    fprintf(stderr, "Process %d: in release received: %d\n", p->id, 1);
    return 1;
}

int all_replies_received(ProcessPtr p)
{
    for (local_id i = 1; i < p->total_processes; i++)
    {
        if (i != p->id && replies_received[i] == 0)
            return 0;
    }
    return 1;
}

void send_cs_request(ProcessPtr p)
{
    Message msg = {.s_header = {MESSAGE_MAGIC, sizeof(local_id), CS_REQUEST, lamport_time}};
    memcpy(msg.s_payload, &p->id, sizeof(local_id));
    send_multicast(p, &msg);
    fprintf(stderr, "Process %d: Sent CS_REQUEST to all at Lamport time %d\n", p->id, lamport_time);
}

void send_cs_reply(ProcessPtr p, local_id to)
{
    Message msg = {.s_header = {MESSAGE_MAGIC, sizeof(local_id), CS_REPLY, lamport_time}};
    memcpy(msg.s_payload, &p->id, sizeof(local_id));
    send(p, to, &msg);
    fprintf(stderr, "Process %d: Sent CS_REPLY to process %d at Lamport time %d\n", p->id, to, lamport_time);
}

void send_cs_release(ProcessPtr p)
{
    Message msg = {.s_header = {MESSAGE_MAGIC, sizeof(local_id), CS_RELEASE, lamport_time}};
    memcpy(msg.s_payload, &p->id, sizeof(local_id));
    send_multicast(p, &msg);
    fprintf(stderr, "Process %d: Sent CS_RELEASE to all at Lamport time %d\n", p->id, lamport_time);
}

int request_cs(const void *self)
{
    ProcessPtr p = (ProcessPtr)self;
    send_cs_request(p);
    add_to_queue(p, p->id, lamport_time);
    queue_enter_time = lamport_time;
    memset(replies_received, 0, sizeof(replies_received));
    fprintf(stderr, "Process %d: Requesting CS at Lamport time %d\n", p->id, lamport_time);
    do
    {
        fprintf(stderr, "Process %d: Waiting for CS - all replies received: %d, is highest priority: %d\n",
                p->id, all_replies_received(p), is_highest_priority(p));
        Message received_msg;
        if (receive_any(p, &received_msg) == 0)
        {
            handle_message(p, &received_msg);
        }
        sort_queue(p);
    } while (all_replies_received(p) == 0 && is_highest_priority(p) == 0);
    fprintf(stderr, "Process %d: Entered CS at Lamport time %d\n", p->id, lamport_time);
    return 0;
}

int release_cs(const void *self)
{
    ProcessPtr p = (ProcessPtr)self;
    send_cs_release(p);
    fprintf(stderr, "Process %d: Released CS at Lamport time %d\n", p->id, lamport_time);
    return 0;
}

int main(int argc, char *argv[])
{
    int child_count = 0, use_mutex = 0;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc)
        {
            child_count = atoi(argv[i + 1]);
        }
        if (strcmp(argv[i], "--mutexl") == 0)
        {
            use_mutex = 1;
        }
    }

    int process_count = child_count + 1;
    global_events_log_file = fopen(events_log, "a");
    if (!global_events_log_file)
        perror("fopen events_log");

    Pipeline *pipeline = create_pipeline(process_count);
    pid_t pids[process_count];
    pids[0] = getpid();

    for (int i = 1; i < process_count; i++)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
        }
        else if (pid == 0)
        {
            local_id my_id = i;
            close_unused_pipes(pipeline, my_id);
            ProcessPtr proc = createProcess(my_id, process_count, pipeline);

            // Отправка сообщения STARTED
            Message msg = {.s_header = {MESSAGE_MAGIC, sizeof(local_id), STARTED, lamport_time}};
            memcpy(msg.s_payload, &proc->id, sizeof(local_id));
            log_started(global_events_log_file, my_id, 0);
            send_multicast(proc, &msg);

            // Получение всех STARTED
            while (!all_started_received)
            {
                Message received_msg;
                if (receive_any(proc, &received_msg) == 0)
                {
                    handle_message(proc, &received_msg);
                }
            }
            log_received_all_started(global_events_log_file, my_id);
            /*
                        for (local_id src = 1; src < process_count; src++)
                        {
                            if (src == my_id)
                                continue;
                            Message rmsg;
                            if (receive(proc, src, &rmsg) != 0)
                            {
                                fprintf(stderr, "Child %d: error receiving STARTED from %d\n", my_id, src);
                            }
                        }
                        log_received_all_started(global_events_log_file, my_id);
            */

            // Выполнение полезной работы
            int N = my_id * 5;
            fprintf(global_events_log_file, "Mutex flag %d", use_mutex);
            if (use_mutex)
            {
                request_cs(proc);
            }
            for (int j = 1; j <= N; j++)
            {
                char message[256];
                snprintf(message, sizeof(message), log_loop_operation_fmt, my_id, j, N);
                fprintf(global_events_log_file, log_loop_operation_fmt, my_id, j, N);
                print(message);
            }
            if (use_mutex)
            {
                release_cs(proc);
            }
            // Обработка ожидающих сообщений
            while (!all_release_received(proc))
            {
                Message received_msg;
                if (receive_any(proc, &received_msg) == 0)
                {
                    handle_message(proc, &received_msg);
                }
            }
            // Отправка сообщения DONE
            msg.s_header.s_magic = MESSAGE_MAGIC;
            msg.s_header.s_payload_len = proc->id;
            msg.s_header.s_type = DONE;
            msg.s_header.s_local_time = lamport_time;
            log_done(global_events_log_file, my_id, 0);
            send_multicast(proc, &msg);

            // Получение всех DONE
            while (!all_done_received)
            {
                Message received_msg;
                if (receive_any(proc, &received_msg) == 0)
                {
                    handle_message(proc, &received_msg);
                }
            }
            log_received_all_done(global_events_log_file, my_id);
            /*
            for (local_id src = 1; src < process_count; src++)
            {
                if (src == my_id)
                    continue;
                Message rmsg;
                if (receive(proc, src, &rmsg) != 0)
                {
                    fprintf(stderr, "Child %d: error receiving DONE from %d\n", my_id, src);
                }
            }
            log_received_all_done(global_events_log_file, my_id);
*/
            free(proc);
            exit(0);
        }
        else
        {
            pids[i] = pid;
        }
    }

    // Родительский процесс
    local_id my_id = 0;
    close_unused_pipes(pipeline, my_id);
    ProcessPtr proc = createProcess(my_id, process_count, pipeline);
    while (!all_started_received)
    {
        Message received_msg;
        if (receive_any(proc, &received_msg) == 0)
        {
            handle_message(proc, &received_msg);
        }
    }
    fprintf(global_events_log_file, "Parent: received all STARTED messages\n");

            while (!all_done_received)
            {
                Message received_msg;
                if (receive_any(proc, &received_msg) == 0)
                {
                    handle_message(proc, &received_msg);
                }
            }
    fprintf(global_events_log_file, "Parent: received all DONE messages\n");

    for (int i = 1; i < process_count; i++)
    {
        wait(NULL);
    }

    fclose(global_events_log_file);
    free(proc);
    free(pipeline);
    return 0;
}
