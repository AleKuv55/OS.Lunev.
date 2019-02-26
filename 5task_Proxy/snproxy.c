#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <sys/wait.h>

#define CHILD_SIZE 4096
#define PULL_SIZE 512

#define _str(x) #x

#define str(x) _str(x)

#define $(code...)                                  \
    do {                                            \
        errno = 0;                                  \
        code;                                       \
        if(errno != 0)                              \
        {                                           \
            perror(#code "at " str(__LINE__));      \
            exit(EXIT_FAILURE);                     \
        }                                           \
    } while(0)

#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

#define COUNTSIZE(counter, n)               \
    ({                                      \
        1024*pow(3, n - counter);           \
    })                                      \

#define close(fd)           \
    do {                    \
        close(fd);          \
        fd = -1;            \
    } while(0)

enum
{
    RD = 0,
    WR = 1
};

struct data
{
    int pipefd_p[2];    //parent writes
    int pipefd_c[2];    //child writes
    char* buf_prnt;     //parent buffer
    int isdata;
    int offset;
    int buf_sz;
};


int TheProxy(FILE* log, int n, int fd_file, FILE* logchld);

int Child(FILE* logchld, int i, struct data* buf);

int Parent(FILE* log, int n, struct data* buf, int lastchild_pid);

struct data* CreateServer(FILE* log, int n, int fd_file);

int PrintStructure(FILE* log, struct data* elem);

int main(int argc, char* argv[])
{
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    if (argc != 3)
    {
        fprintf(stdout, "Please write ./a.out __file__ __n__ ");
        exit(0);
    }
    FILE* log = 0;
    FILE* logchld = 0;

    int fd_file = 0;

    log   = fopen("log", "w+");
    setbuf(log, NULL);
    logchld   = fopen("logchld", "w+");
    setbuf(logchld, NULL);

    fd_file = open(argv[1], O_RDONLY);

    fprintf(log, "fd = %d", fd_file);

    if (fd_file == -1)
    {
        perror("Opening file");
        exit(EXIT_FAILURE);
    }

    int n = 0;

    sscanf(argv[2], "%d", &n);

    TheProxy(log, n, fd_file, logchld);

    return 0;
}

int TheProxy (FILE* log, int n, int fd_file, FILE* logchld)
{
    int i = 1;
    int c_pid = 0;
    int lastchild_pid = 0;
     
    struct data* buf = CreateServer(log, n, fd_file);

    for (i = 1; i <= n ; i++)
    {
        $( c_pid = fork() );

        if (c_pid == 0)
        {
            for (int j = 0; j <= n + 1; j++)
            {
                free(buf[j].buf_prnt);

                close(buf[j].pipefd_p[WR]);
                close(buf[j].pipefd_c[RD]); 
                
                if(i != j)
                { 
                    close(buf[j].pipefd_c[WR]);
                    close(buf[j].pipefd_p[RD]);                    
                }

                buf[j].isdata = 0;
                buf[j].offset = 0;
            }

            fprintf(logchld, "I'm a %d child", i);
            fflush(logchld);
            PrintStructure(logchld, &buf[i]);

            Child(logchld, i, buf);
            break;
        }
        else if (c_pid > 0) 
        {
            fcntl(buf[i].pipefd_p[WR], F_SETFL, O_NONBLOCK | O_WRONLY);
            fcntl(buf[i].pipefd_c[RD], F_SETFL, O_NONBLOCK | O_RDONLY);

       
            close(buf[i].pipefd_p[RD]);
            close(buf[i].pipefd_c[WR]);

        }
    }
        
    if(c_pid > 0)
        Parent(log, n, buf, lastchild_pid);

    return 0;
}

int Child (FILE* logchld, int i, struct data* buf)
{
    char buf_chld[CHILD_SIZE] = {};
    int wasread = 1, waswritten = 1;

    while (wasread != 0)                        
    {   
        wasread = read(buf[i].pipefd_p[RD], buf_chld, CHILD_SIZE);

        if (wasread == -1)
        {
            fprintf(logchld, "Error read, child = %d", i); 
            fflush(logchld);
            perror("C_read, error");
        }
        else if (wasread == 0) //parent was finished
        {
            fprintf(logchld, "wasread = %d Goodbuy CHILD\n", wasread);
            fflush(logchld);
            break; 
        }
        else
        {
            fprintf(logchld, "%d. C_wasread = %d\n", i, wasread);               
            fflush(logchld);
        }

        waswritten = write(buf[i].pipefd_c[WR], buf_chld, wasread);

        if (waswritten == -1) //error or parent was finished
        {
            fprintf(logchld, " child = %d", i); 
            perror("C_write, error");
            break;
        }
        else
        {
            fprintf(logchld, "%d. C_waswritten = %d\n", i, waswritten);
            fflush(logchld);
        }
   
    }

    close(buf[i].pipefd_p[RD]);
    close(buf[i].pipefd_c[WR]);
        
    fprintf(logchld, "Child № %d has finished\n");
    fflush(logchld);

    free(buf);
    exit(0);
}

int Parent (FILE* log, int n, struct data* buf, int lastchild_pid)
{
    int wstatus = 0;
 
    fd_set wrfds;
    fd_set rdfds;

    int wasread = 1, waswritten = 1, i = 0, offsetclose = 0;

    while (1)
    {
        FD_ZERO(&wrfds);
        FD_ZERO(&rdfds);

        int counter = 0, nfds = 0;
        for (i = 0; i <= n + 1; i++)
        {
            if (i != 0 && buf[i].isdata == 0 && buf[i-1].pipefd_c[RD] != -1)
            {
                nfds = max(nfds, buf[i-1].pipefd_c[RD]);

                FD_SET(buf[i-1].pipefd_c[RD], &rdfds);
                counter++;
            }

            if (buf[i].isdata != 0 && buf[i].pipefd_p[WR] != -1) //wasread != 0
            {
                nfds = max(nfds, buf[i].pipefd_p[WR]);

                FD_SET(buf[i].pipefd_p[WR], &wrfds);
                counter++;
            }

        }

        if (counter == 0)
            break;

        $(select((nfds + 1), &rdfds, &wrfds, NULL, NULL));

        for (i = 0; i <= n + 1; i++)
        {
            if (FD_ISSET(buf[i].pipefd_c[RD], &rdfds))
            {
                wasread = read(buf[i].pipefd_c[RD], buf[i+1].buf_prnt, buf[i+1].buf_sz);
                buf[i+1].isdata += wasread;

                if (wasread == 0)
                {
                    close(buf[i].pipefd_c[RD]);

                    close(buf[i+1].pipefd_p[WR]);
                }
            }
            
            if (FD_ISSET(buf[i].pipefd_p[WR], &wrfds))
            {
                waswritten = write(buf[i].pipefd_p[WR],
                        buf[i].buf_prnt + buf[i].offset, buf[i].isdata);
                
                buf[i].isdata  -= waswritten;
                buf[i].offset  += waswritten;

                if (buf[i].isdata == 0)
                    buf[i].offset =  0;
                
            }
        }
    }

   return 0;
}

struct data* CreateServer(FILE* log, int n, int fd_file)
{
    struct data* buf = calloc(n + 2, sizeof(struct data)); 

    buf[0].pipefd_p[RD]   = -1;
    buf[0].pipefd_p[WR]   = -1;
    buf[0].pipefd_c[WR]   = -1;

    buf[0].pipefd_c[RD]   = fd_file;
    buf[0].isdata = 0;
        PrintStructure(log, buf);

    buf[n+1].pipefd_p[RD] = -1;
    buf[n+1].pipefd_c[RD] = -1;
    buf[n+1].pipefd_c[WR] = -1;

    buf[n+1].pipefd_p[WR] = STDOUT_FILENO;
    buf[n+1].buf_sz = COUNTSIZE(n,n);
    buf[n+1].buf_prnt = calloc(buf[n+1].buf_sz, sizeof(char));
    buf[n+1].isdata = 0;
        PrintStructure(log, &buf[n+1]);

    for ( int i = 1; i <= n; i++)
    {
        pipe(buf[i].pipefd_p);
        pipe(buf[i].pipefd_c);

        buf[i].buf_sz = COUNTSIZE(i,n);
        buf[i].buf_prnt = calloc(buf[i].buf_sz, sizeof(char));

        buf[i].isdata = 0;
        buf[i].offset = 0;

        PrintStructure(log, &buf[i]);
    }

    fflush(log);
    return buf;
}

int PrintStructure(FILE* log, struct data* elem)
{
    fprintf(log, "//Print Structure, Pointer = %d\n//", elem);
    fprintf(log, "\tpipefd_p[0] = %d ",  elem -> pipefd_p[0]);
    fprintf(log, "\tpipefd_p[1] = %d\n", elem -> pipefd_p[1]);

    fprintf(log, "\tpipefd_c[0] = %d ",  elem -> pipefd_c[0]);
    fprintf(log, "\tpipefd_c[1] = %d\n", elem -> pipefd_c[1]);
    
    fprintf(log, "\tbuf_prnt = %p\n", elem -> buf_prnt);

    fprintf(log, "\tisdata = %d", elem -> isdata);
    fprintf(log, "\toffset = %d\n", elem -> offset);

    fprintf(log, "//End Print Structure//\n");
    
    fflush(log);
    return 0;
}


