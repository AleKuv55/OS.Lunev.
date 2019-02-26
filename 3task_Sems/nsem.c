#include <stdio.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

int Writer( FILE* log, int sem_id, int shm_id, void* shm_p, FILE* pFile );
int Reader( FILE* log, int sem_id, int shm_id, void* shm_p );

int SemCreation ( FILE* log );
int ShmCreation ( FILE* log );

int SemDeleting ( FILE* log, int sem_id );
int ShmDeleting ( FILE* log, int shm_id );

void* ShmAttaching( FILE* log, int shm_id );


enum Semaphores 
{
    sem_fill,
    sem_empty, 
    sem_wrblock,
    sem_rdblock,
    sem_wrkilled,
    sem_rdkilled,
    sem_number 
};

#define _str(x) #x
#define str(x) _str(x)
#define $(code...)                                                  \
    do {                                                            \
        errno = 0;                                                  \
        code;                                                       \
        if(errno != 0) {                                            \
            perror(#code " at " str(__LINE__));                     \
            exit(EXIT_FAILURE);                                     \
        }                                                           \
    } while(0)
#define PRINT_SEMS(sem_id, sem_number)                                          \
    {                                                                           \
        fprintf(log, "// PRINT_SEMAPHORES //\n");                            \
        fprintf(log,                                                         \
        "\tsem_fill = %d\n\tsem_empty = %d\n\tsem_wrblock = %d\n\tsem_rdblock = %d\n\tsem_wr_killed = %d\n\tsem_rd_killed = %d\n\t",\
           sem_fill, sem_empty, sem_wrblock,                                    \
            sem_rdblock, sem_wrkilled, sem_rdkilled);                         \
        for (int i = 0; i < sem_number; i++)                                    \
        {                                                                       \
            fprintf(log, "\tVal sem №%d = %d\n",                             \
                    i, semctl(sem_id, i, GETVAL));                              \
        }                                                                       \
        fprintf(log, "// FINISHED PRINTED //\n");                            \
    }           


int main( int argc, char* argv[] )
{
    setbuf(stderr, NULL);
    setbuf(stdout, NULL);

    FILE* log = 0;
    FILE* pFile = 0;
    
    log = fopen("log", "w+");
    setbuf(log, NULL);

    if (argc != 3)
    {
       fprintf(log, "Wrong arguments\n");
       exit(0);
    }
    
    pFile = fopen(argv[1], "rb");
    if (pFile == 0)
    {
        perror("1. Opening file");
    }

    int sem_id = SemCreation(log);
    int shm_id = ShmCreation(log);

    void* shm_p = ShmAttaching(log, shm_id);

    int arg = 0;
    sscanf(argv[2], "%d", &arg);    

    switch(arg)
    {
        case 1: //Writer
        {
            printf("Writer\n");
            Writer(log, sem_id, shm_id, shm_p, pFile);
            break;
        }
        case 2: //Reader
        { 
            printf("Reader\n");
            Reader(log, sem_id, shm_id, shm_p);
            break;
        }
        default:
        {
            fprintf(log, "Please choose wr or read\n");
            break;
        }
    }

   // SemDeleting( log, sem_id );
   // ShmDeleting( log, shm_id );
        
    return 0;
}

#define SHMSIZE 64

#define SEMOP_SET(index, num, operation, flag) \
    {                                          \
        semops[index].sem_num = num;           \
        semops[index].sem_op  = operation;     \
        semops[index].sem_flg = flag;          \
        index++;                               \
    }

#define SEM_ERROR(sem_res)                                                  \
    ({                                                                       \
        if (sem_res == -1)                                                  \
        {                                                                   \
            perror("IPC_NOWAIT");                                           \
            if (errno == EAGAIN)                                            \
            {                                                               \
                fprintf(log, "Romeo has died Julluet must kill herself");   \
                exit(0);                                                    \
            }                                                               \
        }                                                                   \
    })


int Reader( FILE* log, int sem_id, int shm_id, void* shm_p )
{
    struct sembuf semops[sem_number] = {};

    char a[SHMSIZE] = {};

    int waswritten = 1, wasread = 0, sem_res = 0, index = 0;

    index = 0;
    SEMOP_SET(index, sem_rdblock, 0, 0);
    SEMOP_SET(index, sem_rdblock, 1, SEM_UNDO);
        SEMOP_SET(index, sem_wrkilled, 0 , 0);
          semop(sem_id, semops, index);

    index = 0;
    SEMOP_SET(index, sem_wrblock, -1, 0);
    SEMOP_SET(index, sem_wrblock, 1, 0);
    SEMOP_SET(index, sem_rdkilled, 1, SEM_UNDO);
    semop(sem_id, semops, index);

    //empty make null
    index = 0;
    SEMOP_SET(index, sem_empty,  1, SEM_UNDO);
    SEMOP_SET(index, sem_empty, -1, 0); 
    semop(sem_id, semops, index);
    
    //fill make null
    index = 0;  
    SEMOP_SET(index, sem_fill,  1, SEM_UNDO);
    SEMOP_SET(index, sem_fill, -1, 0);
    semop(sem_id, semops, index);
   

    do
    {
        // FILL -1
        index = 0;
        SEMOP_SET(index, sem_wrblock,  -1, IPC_NOWAIT);
        SEMOP_SET(index, sem_wrblock,   1, IPC_NOWAIT);
            SEMOP_SET(index, sem_fill, -1, 0);
        semop(sem_id, semops, index);
            SEM_ERROR(sem_res);

//CRITICAL SECTION
            
        //From Buffer to STDOUT
        waswritten = write(fileno(stdout), a, wasread); 

        //From Shared Memory to Buffer a
        wasread = *((ssize_t*)shm_p);
        memcpy(a, shm_p + sizeof(ssize_t), wasread); 

//CRITICAL SECTION

        // EMPTY +1
            index = 0;
            SEMOP_SET(index, sem_empty, 1, 0);
        semop(sem_id, semops, index);

    } while (wasread != 0);

/*    index = 0;
    SEMOP_SET(index, sem_rdblock, -1, SEM_UNDO);
    semop(sem_id, semops, index);
*/
    PRINT_SEMS(sem_id, sem_number);
    return 0;
}

int Writer( FILE* log, int sem_id, int shm_id, void* shm_p, FILE* pFile )
{
    struct sembuf semops[sem_number] = {};
    char buffer[SHMSIZE] = {}; 
    int  wasread = 0, sem_res = 0, index = 0;

// CRITICAL SECTION    
    index = 0;
    SEMOP_SET(index, sem_wrblock, 0, 0);
    SEMOP_SET(index, sem_wrblock, 1, SEM_UNDO); // Need SEM_UNDO because if he waits in 201 
    SEMOP_SET(index, sem_rdkilled, 0, 0);        // For the good terminating     
    semop(sem_id, semops, index);


    index = 0;
    SEMOP_SET(index, sem_rdblock, -1,  0);
    SEMOP_SET(index, sem_rdblock,  1,  0); 
    SEMOP_SET(index, sem_wrkilled, 1,  SEM_UNDO);
    semop(sem_id, semops, index);

//CRITICAL SECTION

    //empty make null
    index = 0;
    SEMOP_SET(index, sem_empty,  1, SEM_UNDO);
    SEMOP_SET(index, sem_empty, -1, 0); 
    semop(sem_id, semops, index);
    
    //fill make null
    index = 0;  
    SEMOP_SET(index, sem_fill,  1, SEM_UNDO);
    SEMOP_SET(index, sem_fill, -1, 0);
    semop(sem_id, semops, index);
   
        
    do
    { 
//CRITICAL SECTION 
        
        //From File to Buffer
        wasread = read(fileno(pFile), buffer, SHMSIZE);
        
        //From Buffered to Shared Memory
        *((ssize_t*)shm_p) = wasread; 
        memcpy(shm_p + sizeof(ssize_t), buffer, wasread);

//CRITICAL SECTION
    
        //FILL +1 Reader can go
        index = 0;
        SEMOP_SET(index, sem_fill, 1, 0); //NO SEM_UNDO, counter per process        
        semop(sem_id, semops, index);

        //EMPTY -1 WAIT HERE
        index = 0;
        SEMOP_SET(index, sem_rdblock,   -1, IPC_NOWAIT);
        SEMOP_SET(index, sem_rdblock,    1, IPC_NOWAIT);   
            SEMOP_SET(index, sem_empty, -1, 0);
        sem_res = semop(sem_id ,semops, index);
            SEM_ERROR(sem_res);

    } while (wasread != 0);

    /*
    index = 0;
    SEMOP_SET(index, sem_wrblock, -1, SEM_UNDO);
    semop(index, semops, index);
    */
    PRINT_SEMS(sem_id, sem_number);
    
    printf("The mission was completed!\n");
    return 0;
}

int SemCreation( FILE* log )
{
    int sem_id = semget(1234, sem_number, IPC_CREAT | 0666);

    if (sem_id == -1)
    {
        perror("Error, semget");
    }
    else 
    {
        fprintf(log, "sem_id = %d\n", sem_id);
    }

    return sem_id;
}

int SemDeleting( FILE* log, int sem_id )
{
    int sem_rm = semctl(sem_id, -1000, IPC_RMID);
    if (sem_rm == -1)
    {
        perror("Error, Sem remove");
    }
    else
    {
        fprintf(log, "Sem was deleted, %d\n", sem_rm);
    }

    return 0;
}

int ShmCreation( FILE* log )
{
    int shm_id = shmget(1234, SHMSIZE, IPC_CREAT | 0666); 

    if (shm_id == -1)
    {
        perror("Error, shmget");
    }
    else
    {
        fprintf(log, "shm_id = %d\n", shm_id);
    }

    return shm_id;
}

int ShmDeleting( FILE* log, int shm_id )
{
    int shm_rm = shmctl(shm_id, IPC_RMID, 0);
    if (shm_rm == -1)
    {
        perror("Error, Shm remove");
    }
    else
    {
        fprintf(log, "Shm was deleted %d\n", shm_rm);
    }
 
    return 0;
}


void* ShmAttaching( FILE* log, int shm_id )
{ 
    void* shm_p = shmat(shm_id, NULL, 0);
    if (shm_p == -1) 
    {
        perror("Error, shmat");                         
    }
    else
    {
        fprintf(log, "shm_p = %p\n", shm_p);
    }
    return shm_p;
} 

/* //empty make null
    SEMOP_SET(sem_empty,     sem_empty,  1, SEM_UNDO);
    SEMOP_SET(sem_empty + 1, sem_empty, -1, 0);
    sem_res = semop(sem_id, semops + sem_empty, 2);
    fprintf(log, "0.1 Sem_wr_killed read BUGS(semop). sem_res = %d\n", sem_res); 
*/
 
/*    //fill make null
    SEMOP_SET(sem_fill,     sem_fill,  1, SEM_UNDO);
    SEMOP_SET(sem_fill + 1, sem_fill, -1, 0);
    sem_res = semop(sem_id, semops + sem_fill, 2); 
    fprintf(stdout, "0.1 Sem_wr_killed read BUGS(semop). sem_res = %d\n", sem_res); 
*/
    

/*    // Block for other readers
    SEMOP_SET(sem_rdblock,     sem_rdblock, 0, 0);
    SEMOP_SET(sem_rdblock + 1, sem_rdblock, 1, SEM_UNDO);
    SEMOP_SET(sem_rdblock + 2, sem_wr_killed, 0, 0);
    sem_res = semop(sem_id, semops + sem_rdblock, 3);
    fprintf(log, "0. Other readers will wait. sem_res = %d\n", sem_res); 


    //semaphore if some process dies + wait writers
    SEMOP_SET(sem_rd_killed,         sem_wrblock,  -1, 0);
    SEMOP_SET(sem_rd_killed + 1,     sem_wrblock,   1, 0);
    SEMOP_SET(sem_rd_killed + 2,     sem_rd_killed, 1, SEM_UNDO);
    sem_res = semop(sem_id, semops + sem_rd_killed, 3);
    fprintf(log, "0.1 Sem_rd_killed read BUGS(semop). sem_res = %d\n", sem_res); 

    //fill make null
    SEMOP_SET(sem_fill,     sem_fill,  1, SEM_UNDO);
    SEMOP_SET(sem_fill + 1, sem_fill, -1, 0);
    sem_res = semop(sem_id, semops + sem_fill, 2); 

    //empty make null
    SEMOP_SET(sem_empty,     sem_empty,  1, SEM_UNDO);
    SEMOP_SET(sem_empty + 1, sem_empty, -1, 0); 
    sem_res = semop(sem_id, semops + sem_empty, 2);
 
    PRINT_SEMS(sem_id, sem_number);

    //Critical section
    while(wasread > 0)
    {
        //FILL -1 
       // SEMOP_SET(sem_fill,     sem_wr_killed, -1, IPC_NOWAIT);
       // SEMOP_SET(sem_fill + 1, sem_wr_killed,  1, IPC_NOWAIT);
        SEMOP_SET(sem_fill, sem_fill,      -1, SEM_UNDO);//SEM_UNDO?
        sem_res = semop(sem_id, semops + sem_fill, 1);
        if(sem_res == -1)
        {
            perror("IPC_NOWAIT");
            if(errno == EAGAIN)
            {    
                exit(0);
            }
        }
        else
        {
            fprintf(log, "\n1. Sem_fill -= 1, sem_res = %d\n", sem_res);
        }

        //From Shared Memory to Buffer a
        wasread = *((ssize_t*)shm_p);
        fprintf(log, "2. wasread = %d\n", wasread);
        void* desd = memcpy(a, shm_p + sizeof(ssize_t), wasread);
        //sleep(20);
        //EMPTY +1
        SEMOP_SET(sem_empty,     sem_wr_killed, -1, IPC_NOWAIT);
        SEMOP_SET(sem_empty + 1, sem_wr_killed,  1, IPC_NOWAIT);
        SEMOP_SET(sem_empty + 2, sem_empty,      1, SEM_UNDO);//SEM_UNDO

        sem_res = semop(sem_id, semops + sem_empty, 3);
        if (sem_res == -1)
        {
            perror("IPC_NOWAIT");
            if (errno == EAGAIN)
            {   
                fprintf(log, "Romeo has died Julluet must kill herself");    
                exit(0);
            }
        }
        else
        {
            fprintf(log, "3. Sem_empty = 1, sem_res = %d\n", sem_res);
        }

        //From Buffer to STDOUT
        waswritten = write(fileno(stdout), a, wasread);
        fprintf(log, "4. Waswritten = %d\n", waswritten);
    }
 
    //UNBLOCK other readers
    SEMOP_SET(sem_rdblock, sem_rdblock, -1, SEM_UNDO);
    sem_res = semop(sem_id, semops + sem_rdblock, 1);
    fprintf(log, "7. Other readers can go. sem_res = %d\n", sem_res); 
*/
  
/*    //semaphore if some process dies
    SEMOP_SET(sem_rd_killed,     sem_rd_killed,  1, SEM_UNDO);
    SEMOP_SET(sem_rd_killed + 1, sem_rd_killed, -1, 0);//WTF IS GOING ON
    sem_res = semop(sem_id, semops + sem_rd_killed, 2);
    fprintf(log, "0.1 Sem_rd_killed read BUGS(semop). sem_res = %d\n", sem_res); 
*/
  /*   
    // Block for other writers
    SEMOP_SET(sem_wrblock,     sem_wrblock, 0, 0);
    SEMOP_SET(sem_wrblock + 1, sem_wrblock, 1, SEM_UNDO);
    SEMOP_SET(sem_wrblock + 2, sem_rd_killed, 0, 0);
    sem_res = semop(sem_id, semops + sem_wrblock, 3);
    fprintf(stdout, "0. Other writers will wait. sem_res = %d\n", sem_res); 
    
    //semaphore if some process dies + wait readers
    SEMOP_SET(sem_wr_killed,         sem_rdblock,  -1, 0);
    SEMOP_SET(sem_wr_killed + 1,     sem_rdblock,   1, 0);
    SEMOP_SET(sem_wr_killed + 2,     sem_wr_killed, 1, SEM_UNDO);
    sem_res = semop(sem_id, semops + sem_wr_killed, 3);
    fprintf(stdout, "0.1 Sem_wr_killed read BUGS(semop). sem_res = %d\n", sem_res); 
*/
/*    //empty make null
    SEMOP_SET(sem_empty,     sem_empty,  1, SEM_UNDO);
    SEMOP_SET(sem_empty + 1, sem_empty, -1, 0); 
    sem_res = semop(sem_id, semops + sem_empty, 2);
    fprintf(log, "0.1 Sem_wr_killed read BUGS(semop). sem_res = %d\n", sem_res); 
*/
    /*
    //fill make null
    SEMOP_SET(sem_fill,     sem_fill,  1, SEM_UNDO);
    SEMOP_SET(sem_fill + 1, sem_fill, -1, 0);
    sem_res = semop(sem_id, semops + sem_fill, 2);

    PRINT_SEMS(sem_id, sem_number);

    //Start critical section
    while (wasread > 0)
    {
        //From File to Buffer
        wasread = read(fileno(pFile), buffer, SHMSIZE);
        if (wasread == -1)
        {
            perror("Error, read");
        }
        else 
        {
           printf("2. Wasread = %d\n", wasread);
        }

        //From Buffered to Shared Memory
        *((ssize_t*)shm_p) = wasread;
        printf("4. *Shm_p = %p\n", shm_p);
        
        void* desd = memcpy(shm_p + sizeof(ssize_t), buffer, wasread);
       
       
        //FILL +1 
            
        SEMOP_SET(sem_fill,     sem_rd_killed, -1, IPC_NOWAIT);
        SEMOP_SET(sem_fill + 1, sem_rd_killed, 1, IPC_NOWAIT);
        SEMOP_SET(sem_fill + 2, sem_fill,      1, SEM_UNDO);

        sem_res = semop(sem_id, semops + sem_fill, 3);
        if(sem_res == -1)
        {
            perror("IPC_NOWAIT");
            if(errno == EAGAIN)
            {   
                fprintf(stdout, "Romeo has died Julluet must kill herself");    
                exit(0);
            }
        }
        else
        {
            printf("5. Sem_fill = 1, sem_res = %d\n", sem_res);
            fflush(stdout);
        }

        //EMPTY -1. The process will wait Reader here
       
        PRINT_SEMS(sem_id, sem_number);
       
        SEMOP_SET(sem_empty,     sem_rd_killed,  -1, IPC_NOWAIT);
        SEMOP_SET(sem_empty + 1, sem_rd_killed,   1, IPC_NOWAIT);
        SEMOP_SET(sem_empty + 2, sem_empty,      -1, SEM_UNDO);//SEM_UNDO?
        sem_res = semop(sem_id, semops + sem_empty, 3);
        if (sem_res == -1)
        {
            perror("IPC_NOWAIT");
            if (errno == EAGAIN)
            {   
                fprintf(stdout, "Romeo has died Julluet must kill herself");    
                exit(0);
            }
        }
        else
        {
            printf("6. Sem_empty -= 1, sem_res = %d\n", sem_res);
        }
        printf("\n");
    }

    // Let other writers go
    SEMOP_SET(sem_wrblock, sem_wrblock, -1, SEM_UNDO);
    sem_res = semop(sem_id, semops + sem_wrblock, 1);
    fprintf(stdout, "7. Other writers can go. sem_res = %d\n", sem_res); 
*/

