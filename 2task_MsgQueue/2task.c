#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

#define MSGSZ 16

struct msgbuf
{
    long mtype;
    char mtext[MSGSZ];
};

int SendMessagePrint(FILE* log, int msqid, int counter);

int ReceiveMessagePrint(FILE* log, int msqid, int msgtyp);

int TheProgram(FILE* log, int n, int msqid);

int CreateMessageQueue(FILE* log);

int DeleteMessageQueue(FILE* log, int msqid);

int main(int argc, char* argv[])
{
    FILE* log = 0;
  
    log = fopen("log", "w+");
    
    if (argc == 2)
    {
        int n = 0;

        sscanf(argv[1], "%d", &n);
        fprintf(log, "0. arg, n = %d\n\n", n);   
        
        int msqid = 0;  
        msqid = CreateMessageQueue(log);

        TheProgram(log, n, msqid);

    }
    else
    {
        printf("Wrong argument\n");
    }

    return 0;
}

int TheProgram(FILE* log, int n, int msqid)
{
    pid_t p_pid = getpid();
    fprintf(log, "2. Pid of the parent = %d\n", p_pid);
     
    fflush(log);
    int i = 1;
    int c_pid = 0;
    int type = 0;

    for (i = 1; i <= n; i++)
    {
        c_pid = fork();

        if (c_pid == 0) //Children
        {
            break;
        }
        else if (c_pid > 0) // Parent
        {
            fprintf(log, "3. Parent, i = %d, c_pid = %d\n", i, c_pid);     
            fflush(log);
        }
        else // Error
        {
            perror("Fork error");
        }

    }

    if (c_pid == 0) //Children
    {
        fprintf(log, "4. Child, pid = %d, i = %d \n", getpid(), i);
        fflush(log);        

        type = ReceiveMessagePrint(log, msqid, i);
       
        fflush(stdout);
       
        fprintf(stdout, "%d ", i);
       
        fflush(stdout);

        SendMessagePrint(log, msqid, n + 1);       
        
        exit(0);
    }
    else // Parent
    {
        int wstatus = 0;

        for (int j = 1; j <= n; j++)
        {
            SendMessagePrint(log, msqid, j);   
           
            ReceiveMessagePrint(log, msqid, n + 1);
        }

        DeleteMessageQueue(log, msqid);   
        printf("\n");
    }


    return 0; 
}

int SendMessagePrint(FILE* log, int msqid, int counter)
{
    struct msgbuf msg;
    sprintf(msg.mtext, "%d", counter);
    msg.mtype = counter;

    int msgsnd_res = 0;
    
    msgsnd_res = msgsnd(msqid, (void*) &msg, sizeof(msg.mtext), 0); 
    
    if (msgsnd_res == -1)
    {
        perror("Msgsnd error");
    }
    else      
    {
        fprintf(log, "\t msgsnd_res = %d, msg.text = %s, msg.type = %d \n", msgsnd_res, msg.mtext, msg.mtype);
        fflush(log);
    }

    return 0;
}

int ReceiveMessagePrint(FILE* log, int msqid, int msgtyp)
{
    struct msgbuf msg;

    int copied_bytes = 0;

    copied_bytes =  msgrcv(msqid, (void*) &msg, MSGSZ, msgtyp, 0);
    
    if (copied_bytes == -1)
    {
        perror("Error, receive message");
    }
    else
    {
        fprintf(log, "\t msgrcv, copied bytes = %d, mtext = %s, mtype = %d \n\n", copied_bytes, msg.mtext, msg.mtype);
        fflush(log);
    }
    return msg.mtype;
}

int CreateMessageQueue(FILE* log)
{
    int msqid = 0;
    msqid = msgget(IPC_PRIVATE, 0666);
   
    if (msqid == -1)
    {
        perror("Msg, error");
    }
    else
    { 
        fprintf(log, "1. Msqid = %d\n", msqid);
    } 
    fflush(log);
    return msqid;
}

int DeleteMessageQueue(FILE* log, int msqid)
{
    if (msgctl(msqid, IPC_RMID, NULL) == -1)
    {
        perror("Message queue could not be deleted");
    }
    else
    {
        fprintf(log, "Msq was deleted\n");
    }
    fflush(log);
}

