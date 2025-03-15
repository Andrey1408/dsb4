#define _POSIX_C_SOURCE 200112L
#include "ipc.h"
#include "process_factory.h"
#include "pipe_utils.h"
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>  

// Предполагается, что lamport_time - это глобальная переменная, доступная здесь
extern timestamp_t lamport_time;

/**
 * Задержка на указанное количество миллисекунд.
 *
 * @param milliseconds Количество миллисекунд для задержки
 */
void delay_ms(int milliseconds) {
    struct timespec req;
    req.tv_sec = milliseconds / 1000;
    req.tv_nsec = (milliseconds % 1000) * 1000000;
    while (nanosleep(&req, &req) == -1 && errno == EINTR) {
        // Продолжаем ожидание, если прервано сигналом
    }
}

/**
 * Отправляет сообщение процессу с указанным идентификатором.
 *
 * @param self    Указатель на структуру процесса
 * @param dst     ID получателя
 * @param msg     Сообщение для отправки
 * @return 0 в случае успеха, ненулевое значение при ошибке
 */
int send(void *self, local_id dst, const Message *msg) {
    ProcessPtr p = (ProcessPtr)self;
    Pipeline *pipeline = (Pipeline*)p->pipeline;
    if (dst == p->id || dst >= p->total_processes) {
        return -1;
    }
    int fd = getWriterById(p->id, dst, pipeline);
    if (fd < 0) {
        return -1;
    }

    // Увеличиваем время Лампорта перед отправкой
    lamport_time++;
    Message updated_msg = *msg;
    updated_msg.s_header.s_local_time = lamport_time;

    size_t total_len = sizeof(MessageHeader) + updated_msg.s_header.s_payload_len;
    size_t written = 0;
    while (written < total_len) {
        int ret = write(fd, (char*)&updated_msg + written, total_len - written);
        if (ret > 0) {
            written += ret;
        } else if (ret == 0) {
            return -1;  // Канал закрыт
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                delay_ms(1);  // Ожидаем и повторяем попытку
            } else {
                return -1;
            }
        }
    }
    return 0;
}

/**
 * Отправляет одно и то же сообщение всем другим процессам, включая родительский.
 *
 * @param self    Указатель на структуру процесса
 * @param msg     Сообщение для рассылки
 * @return 0 в случае успеха, ненулевое значение при ошибке
 */
int send_multicast(void *self, const Message *msg) {
    ProcessPtr p = (ProcessPtr)self;
    for (local_id dst = 0; dst < p->total_processes; dst++) {
        if (dst != p->id) {
            if (send(self, dst, msg) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

/**
 * Получает сообщение от процесса с указанным идентификатором.
 *
 * @param self    Указатель на структуру процесса
 * @param from    ID отправителя
 * @param msg     Указатель на структуру для записи сообщения
 * @return 0 в случае успеха, ненулевое значение при ошибке
 */
int receive(void *self, local_id from, Message *msg) {
    ProcessPtr p = (ProcessPtr)self;
    Pipeline *pipeline = (Pipeline*)p->pipeline;
    if (from == p->id || from >= p->total_processes) {
        return -1;
    }
    int fd = getReaderById(from, p->id, pipeline);
    if (fd < 0) {
        return -1;
    }

    size_t header_size = sizeof(MessageHeader);
    size_t readn = 0;
    while (readn < header_size) {
        int ret = read(fd, (char*)&msg->s_header + readn, header_size - readn);
        if (ret > 0) {
            readn += ret;
        } else if (ret == 0) {
            return -1;  // Канал закрыт
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                delay_ms(1);  // Ожидаем и повторяем попытку
            } else {
                return -1;
            }
        }
    }

    // Обновляем время Лампорта после получения заголовка
    lamport_time = (lamport_time > msg->s_header.s_local_time) ? lamport_time + 1 : msg->s_header.s_local_time + 1;

    size_t payload_len = msg->s_header.s_payload_len;
    if (payload_len > MAX_PAYLOAD_LEN) {
        return -1;
    }
    readn = 0;
    while (readn < payload_len) {
        int ret = read(fd, msg->s_payload + readn, payload_len - readn);
        if (ret > 0) {
            readn += ret;
        } else if (ret == 0) {
            return -1;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                delay_ms(1);  // Ожидаем и повторяем попытку
            } else {
                return -1;
            }
        }
    }
    return 0;
}

/**
 * Получает сообщение от любого процесса.
 *
 * @param self    Указатель на структуру процесса
 * @param msg     Указатель на структуру для записи сообщения
 * @return 0 в случае успеха, ненулевое значение при ошибке
 */
int receive_any(void *self, Message *msg) {
    ProcessPtr p = (ProcessPtr)self;
    Pipeline *pipeline = (Pipeline*)p->pipeline;
    while (1) {
        for (local_id from = 0; from < p->total_processes; from++) {
            if (from != p->id) {
                int fd = getReaderById(from, p->id, pipeline);
                if (fd < 0) continue;
                size_t header_size = sizeof(MessageHeader);
                int ret = read(fd, &msg->s_header, header_size);
                if (ret == (int)header_size) {
                    // Обновляем время Лампорта после получения заголовка
                    lamport_time = (lamport_time > msg->s_header.s_local_time) ? lamport_time + 1 : msg->s_header.s_local_time + 1;
                    size_t payload_len = msg->s_header.s_payload_len;
                    if (payload_len > MAX_PAYLOAD_LEN) {
                        continue;
                    }
                    size_t payload_readn = 0;
                    while (payload_readn < payload_len) {
                        ret = read(fd, msg->s_payload + payload_readn, payload_len - payload_readn);
                        if (ret > 0) {
                            payload_readn += ret;
                        } else if (ret == 0) {
                            break;
                        } else {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                break;
                            } else {
                                return -1;
                            }
                        }
                    }
                    if (payload_readn == payload_len) {
                        return 0;
                    }
                } else if (ret > 0) {
                    // Частичное чтение, здесь не обрабатывается
                } else if (ret == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    return -1;
                }
            }
        }
        delay_ms(1);  // Ожидаем перед следующей попыткой
    }
}
