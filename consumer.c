#include "shared_def.h"

void sem_operation(int sem_id, int sem_num, int operation) {
    struct sembuf sem_buf;
    sem_buf.sem_num = sem_num;
    sem_buf.sem_op = operation;
    sem_buf.sem_flg = 0;

    if (semop(sem_id, &sem_buf, 1) == -1) {
        if (errno == EIDRM) {
            printf("Consumer: Semaphores removed, exiting...\n");
            exit(0);
        }
        perror("semop failed");
        exit(1);
    }
}

int main() {
    key_t shm_key_prod = ftok(".", PROJECT_ID_SHM_PRODUCER);
    key_t shm_key_cons = ftok(".", PROJECT_ID_SHM_CONSUMER);
    
    if (shm_key_prod == -1 || shm_key_cons == -1) {
        perror("ftok failed");
        exit(1);
    }

    size_t shm_size = sizeof(struct shared_data);
    int shm_id_prod = shmget(shm_key_prod, shm_size, 0666);
    int shm_id_cons = shmget(shm_key_cons, shm_size, 0666);

    if (shm_id_prod == -1 || shm_id_cons == -1) {
        perror("shmget failed");
        exit(1);
    }

    struct shared_data *prod_mem = (struct shared_data *)shmat(shm_id_prod, NULL, 0);
    struct shared_data *cons_mem = (struct shared_data *)shmat(shm_id_cons, NULL, 0);

    if (prod_mem == (void *)-1 || cons_mem == (void *)-1) {
        perror("shmat failed");
        exit(1);
    }

    key_t sem_key = ftok(".", PROJECT_ID_SEM);
    if (sem_key == -1) {
        perror("ftok failed for semaphore");
        exit(1);
    }

    int sem_id = semget(sem_key, NUM_SEMS, 0666);
    if (sem_id == -1) {
        perror("semget failed");
        exit(1);
    }

    char timestamp[TIMESTAMP_SIZE];
    get_timestamp(timestamp);
    printf("[%s] Consumer started by %s. Waiting for data...\n", 
           timestamp, USER_NAME);

    while (1) {
        sem_operation(sem_id, SEM_FULL_PROD, -1);
        sem_operation(sem_id, SEM_MUTEX_PROD, -1);

        int value = prod_mem->buffer[prod_mem->out];
        get_timestamp(timestamp);
        printf("[%s] Consumer: Read %d from position %d\n", 
               timestamp, value, prod_mem->out);
        
        prod_mem->out = (prod_mem->out + 1) % BUFFER_SIZE;

        sem_operation(sem_id, SEM_MUTEX_PROD, 1);
        sem_operation(sem_id, SEM_EMPTY_PROD, 1);

        // Process the value
        value += 1;
        printf("[%s] Consumer: Processed value to %d\n", timestamp, value);
        
        // Write processed value back
        sem_operation(sem_id, SEM_EMPTY_CONS, -1);
        sem_operation(sem_id, SEM_MUTEX_CONS, -1);

        cons_mem->buffer[cons_mem->in] = value;
        get_timestamp(cons_mem->last_update);
        printf("[%s] Consumer: Written processed value %d back to producer\n", 
               timestamp, value);
        
        cons_mem->in = (cons_mem->in + 1) % BUFFER_SIZE;
        strcpy(cons_mem->last_user, USER_NAME);

        sem_operation(sem_id, SEM_MUTEX_CONS, 1);
        sem_operation(sem_id, SEM_FULL_CONS, 1);
    }

    shmdt(prod_mem);
    shmdt(cons_mem);
    return 0;
}