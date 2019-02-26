#include<signal.h>
#include<stdlib.h>
#include<stdio.h>
#include<errno.h>
#include<unistd.h>
#include<sys/types.h>
#include<string.h>
#include<fcntl.h>
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

#define PRINT_SIG_SET(set)                          \
    ({                                                  \
        i = 1;                                          \
        while(i < 32)                                   \
        {                                               \
            printf("%d ", sigismember(set, i));  \
            i++;                                        \
        }                                               \
        printf("\n");                                   \
    })

#define PRINT_CURR_MASK(oldset)                 \
    ({                                          \
        sigprocmask(SIG_BLOCK, NULL, oldset);    \
        PRINT_SIG_SET(oldset);                  \
     }) 

#define SIG_ACTION(str, function, signal)       \
    ({                                          \
        struct sigaction act_##str;                  \
        memset(&act_##str, 0, sizeof(act_##str));           \
        act_##str.sa_handler = function;              \
        sigfillset(&act_##str.sa_mask);               \
        sigaction(signal, &act_##str, NULL);          \
        })

// SIGCHILD 
void ChildExit(int signo);

//SIGALARM
void ParentExit(int signo);

//Nothing
void Empty(int signo);

// SIGUSR1
void One(int signo);

// SIGUSR2
void Zero(int signo);


int Child(pid_t ppid, char** argv);

int Parent(pid_t ppid);

pid_t pid = 666;
int out_char = 0;
int counter = 1 << 7; //10000000
int breaking = 0;
pid_t ppid = 666;
int sig_chld = 0;

int main(int argc, char* argv[])
{
    setbuf(stdout,  NULL);
    if (argc != 2)
    {
        fprintf(stdout, "Wr.arg ./a.out file.txt");
        exit(EXIT_FAILURE);
    }
    
    ppid = getpid();

    sigset_t set;
    sigset_t oldset;
    int i = 0;

    // SIGCHILD
    struct sigaction act_exit = {};
    act_exit.sa_handler = ChildExit; 
    sigfillset(&act_exit.sa_mask);
    sigaction(SIGCHLD, &act_exit, NULL);

    // SIGUSR1 - One
    struct sigaction act_one;
    memset(&act_one, 0, sizeof(act_one));
    act_one.sa_handler = One; 
    sigfillset(&act_one.sa_mask);
    sigaction(SIGUSR1, &act_one, NULL);

    // SIGUSR2 - Zero
    struct sigaction act_zero;
    memset(&act_zero, 0, sizeof(act_zero));
    act_zero.sa_handler = Zero; 
    sigfillset(&act_zero.sa_mask);
    sigaction(SIGUSR2, &act_zero, NULL);

    sigemptyset(&set);
//sigint
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGALRM);
    
    sigprocmask(SIG_BLOCK, &set, NULL);

    
    PRINT_CURR_MASK(&oldset);

    counter = 128;
    out_char = 0;

    pid = fork();
    
    if (pid == 0)
    {
        Child(ppid, argv);    
    }
    else if (pid > 0)
    {
        Parent(ppid);
    }

    return 0;
}

int Child (pid_t ppid, char** argv)
{
    sigset_t set;
    int fd = 0;     
    char c = 0;

    struct sigaction act_empty;
    memset(&act_empty, 0, sizeof(act_empty));
    act_empty.sa_handler = Empty; 
    sigfillset(&act_empty.sa_mask);
    sigaction(SIGUSR1, &act_empty, NULL);

    struct sigaction act_alarm;
    memset(&act_alarm, 0, sizeof(act_alarm));
    act_alarm.sa_handler = ParentExit; 
    sigfillset(&act_alarm.sa_mask);
    sigaction(SIGALRM, &act_alarm, NULL);

    if ( (fd = open(argv[1], O_RDONLY)) < 0 )
    {
        perror("Can't open file");
        exit(EXIT_FAILURE);
    }

    int i, wasread;
    printf("set in child, fd = %d\n", fd);
    PRINT_SIG_SET(&set);

    sigemptyset(&set);

    while ( (wasread = read(fd, &c, 1)) > 0 )
    {
        for (i = 128; i >= 1; i /= 2)
        {
//stop
            if (i & c)
                kill(ppid, SIGUSR1);   
            else            
                kill(ppid, SIGUSR2);
//start
            breaking = 0;
            while (breaking != 1)
            {
                alarm(1);

// parent vs kernel

                sigsuspend(&set);
                alarm(0);
            }

        }

    }
    exit(EXIT_SUCCESS);
}

int k;
char buf[128] = {};
int Parent(pid_t ppid)
{
    sigset_t set;
    sigemptyset(&set);
    errno = 0;
    k = 0;
    sig_chld = 0;
    do {
        //start
        sigsuspend(&set);
        if (sig_chld == 1)
        {
            write(STDOUT_FILENO, buf, k); // do not use in handlers
            exit(EXIT_SUCCESS);

        }
        if (counter == 0)
        {
            buf[k++] = out_char;
            if (k == sizeof(buf)) 
            {
                write(STDOUT_FILENO, buf, sizeof(buf));
                k = 0;
            }
            counter = 128;
            out_char = 0;
        }
        kill(pid, SIGUSR1);
        //stop
    } while(1);

}
//SIGCHILD
void ChildExit(int signo)
{
    sig_chld = 1;
}

//SIGALARM
void ParentExit(int signo)
{
   if (ppid != getppid()) 
       exit(EXIT_SUCCESS);
}

//Nothing
void Empty(int signo)
{
    breaking = 1;
}

// SIGUSR1
void One(int signo)
{
//    printf("SIGUSR1 in the proccess\n");
    out_char += counter;
    counter /= 2;
}

// SIGUSR2
void Zero(int signo)
{
    counter /= 2;
}

 //  SIG_ACTION(exit, ChildExit, SIGCHLD);
