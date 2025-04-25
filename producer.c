#include "shared_def.h"

struct shared_data *prod_mem = NULL;
struct shared_data *cons_mem = NULL;

void cleanup(int shm_id_prod, int shm_id_cons, int sem_id) {
    if (shm_id_prod != -1) {
        shmctl(shm_id_prod, IPC_RMID, NULL);
    }
    if (shm_id_cons != -1) {
        shmctl(shm_id_cons, IPC_RMID, NULL);
    }
    if (sem_id != -1) {
        semctl(sem_id, 0, IPC_RMID);
    }
}

int create_and_init_semaphores() {
    key_t sem_key = ftok(".", PROJECT_ID_SEM);
    if (sem_key == -1) {
        perror("ftok failed for semaphore");
        exit(1);
    }

    // Try to create new semaphore set
    int sem_id = semget(sem_key, NUM_SEMS, IPC_CREAT | IPC_EXCL | 0666);
    
    if (sem_id != -1) {
        // Successfully created new semaphore set, initialize them
        union semun arg;
        unsigned short values[NUM_SEMS] = {
            BUFFER_SIZE, // SEM_EMPTY_PROD
            0,          // SEM_FULL_PROD
            1,          // SEM_MUTEX_PROD
            BUFFER_SIZE,// SEM_EMPTY_CONS
            0,          // SEM_FULL_CONS
            1           // SEM_MUTEX_CONS
        };
        arg.array = values;

        if (semctl(sem_id, 0, SETALL, arg) == -1) {
            perror("Failed to initialize semaphores");
            cleanup(-1, -1, sem_id);
            exit(1);
        }
    } else if (errno == EEXIST) {
        // Semaphore set already exists, just get the ID
        sem_id = semget(sem_key, NUM_SEMS, 0666);
        if (sem_id == -1) {
            perror("Failed to get existing semaphores");
            exit(1);
        }
    } else {
        perror("Failed to create semaphores");
        exit(1);
    }

    return sem_id;
}

void sem_operation(int sem_id, int sem_num, int operation) {
    struct sembuf sem_buf;
    sem_buf.sem_num = sem_num;
    sem_buf.sem_op = operation;
    sem_buf.sem_flg = IPC_NOWAIT;

    if (semop(sem_id, &sem_buf, 1) == -1) {
        if (errno == EAGAIN) {
            if (sem_num == SEM_EMPTY_PROD && operation < 0) {
                char timestamp[TIMESTAMP_SIZE];
                get_timestamp(timestamp);
                printf("[%s] Buffer is full! Please wait for consumer to read values.\n", 
                       timestamp);
            }
        } else {
            perror("semop failed");
            exit(1);
        }
    }
}

int main() {
    // Clean up any existing IPC resources first
    system("ipcrm -a >/dev/null 2>&1");
    
    key_t shm_key_prod = ftok(".", PROJECT_ID_SHM_PRODUCER);
    key_t shm_key_cons = ftok(".", PROJECT_ID_SHM_CONSUMER);
    
    if (shm_key_prod == -1 || shm_key_cons == -1) {
        perror("ftok failed");
        exit(1);
    }

    size_t shm_size = sizeof(struct shared_data);
    int shm_id_prod = shmget(shm_key_prod, shm_size, IPC_CREAT | 0666);
    int shm_id_cons = shmget(shm_key_cons, shm_size, IPC_CREAT | 0666);

    if (shm_id_prod == -1 || shm_id_cons == -1) {
        perror("shmget failed");
        cleanup(shm_id_prod, shm_id_cons, -1);
        exit(1);
    }

    prod_mem = (struct shared_data *)shmat(shm_id_prod, NULL, 0);
    cons_mem = (struct shared_data *)shmat(shm_id_cons, NULL, 0);

    if (prod_mem == (void *)-1 || cons_mem == (void *)-1) {
        perror("shmat failed");
        cleanup(shm_id_prod, shm_id_cons, -1);
        exit(1);
    }

    // Initialize shared memory
    prod_mem->in = 0;
    prod_mem->out = 0;
    get_timestamp(prod_mem->last_update);
    strcpy(prod_mem->last_user, USER_NAME);

    cons_mem->in = 0;
    cons_mem->out = 0;
    get_timestamp(cons_mem->last_update);
    strcpy(cons_mem->last_user, USER_NAME);

    // Create and initialize semaphores
    int sem_id = create_and_init_semaphores();

    printf("Current Date and Time (UTC - YYYY-MM-DD HH:MM:SS formatted): %s\n", 
           prod_mem->last_update);
    printf("Current User's Login: %s\n", prod_mem->last_user);
    printf("Buffer size: %d\n", BUFFER_SIZE);
    printf("Enter numbers (-1 to exit):\n");

    while (1) {
        int input;
        scanf("%d", &input);

        if (input == -1) {
            break;
        }

        struct sembuf ops[2];
        ops[0].sem_num = SEM_EMPTY_PROD;
        ops[0].sem_op = -1;
        ops[0].sem_flg = IPC_NOWAIT;
        
        ops[1].sem_num = SEM_MUTEX_PROD;
        ops[1].sem_op = -1;
        ops[1].sem_flg = IPC_NOWAIT;

        if (semop(sem_id, ops, 2) == -1) {
            if (errno == EAGAIN) {
                get_timestamp(prod_mem->last_update);
                printf("[%s] Producer buffer is full! Please wait.\n", 
                       prod_mem->last_update);
                continue;
            }
            perror("semop failed");
            break;
        }

        prod_mem->buffer[prod_mem->in] = input;
        get_timestamp(prod_mem->last_update);
        
        printf("[%s] Producer: Written %d at position %d\n", 
               prod_mem->last_update, input, prod_mem->in);
        
        prod_mem->in = (prod_mem->in + 1) % BUFFER_SIZE;

        ops[0].sem_num = SEM_MUTEX_PROD;
        ops[0].sem_op = 1;
        ops[0].sem_flg = 0;
        
        ops[1].sem_num = SEM_FULL_PROD;
        ops[1].sem_op = 1;
        ops[1].sem_flg = 0;

        if (semop(sem_id, ops, 2) == -1) {
            perror("semop failed");
            break;
        }

        // Check for processed data from consumer
        if (semctl(sem_id, SEM_FULL_CONS, GETVAL) > 0) {
            sem_operation(sem_id, SEM_FULL_CONS, -1);
            sem_operation(sem_id, SEM_MUTEX_CONS, -1);

            int processed_value = cons_mem->buffer[cons_mem->out];
            get_timestamp(cons_mem->last_update);
            printf("[%s] Producer: Received processed value %d from consumer\n", 
                   cons_mem->last_update, processed_value);

            cons_mem->out = (cons_mem->out + 1) % BUFFER_SIZE;

            sem_operation(sem_id, SEM_MUTEX_CONS, 1);
            sem_operation(sem_id, SEM_EMPTY_CONS, 1);
        }
    }

    printf("[%s] Producer: Terminating...\n", prod_mem->last_update);
    
    shmdt(prod_mem);
    shmdt(cons_mem);
    cleanup(shm_id_prod, shm_id_cons, sem_id);
    return 0;
}