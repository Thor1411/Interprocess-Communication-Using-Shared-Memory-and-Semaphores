#ifndef SHARED_DEF_H
#define SHARED_DEF_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include <time.h>
#include <string.h>

#define PROJECT_ID_SHM_PRODUCER 'S'
#define PROJECT_ID_SHM_CONSUMER 'C'
#define PROJECT_ID_SEM 'M'
#define BUFFER_SIZE 5
#define TIMESTAMP_SIZE 30
#define USER_NAME "Thor1411"

// Structure to be shared between processes
struct shared_data {
    int buffer[BUFFER_SIZE];
    int in;
    int out;
    char last_update[TIMESTAMP_SIZE];
    char last_user[50];
};

// Semaphore indices
#define SEM_EMPTY_PROD 0
#define SEM_FULL_PROD 1
#define SEM_MUTEX_PROD 2
#define SEM_EMPTY_CONS 3
#define SEM_FULL_CONS 4
#define SEM_MUTEX_CONS 5
#define NUM_SEMS 6

// Define union semun explicitly
union semun {
    int val;                  /* Value for SETVAL */
    struct semid_ds *buf;     /* Buffer for IPC_STAT, IPC_SET */
    unsigned short *array;    /* Array for GETALL, SETALL */
    struct seminfo *__buf;    /* Buffer for IPC_INFO (Linux-specific) */
};

void get_timestamp(char* timestamp) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(timestamp, TIMESTAMP_SIZE, "%Y-%m-%d %H:%M:%S", tm_info);
}

#endif