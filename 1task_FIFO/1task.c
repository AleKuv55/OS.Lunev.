#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <signal.h>

#define SIZE 100

int Reader(int* fd_GF, FILE* logread);

int Writer(int* ptr_fd_GF, FILE* pFile, FILE* logwrite);

int CreateGeneralFIFO();

int main(int argc, char* argv[])
{	
    int fd_GF = 0;
    FILE* pFile = 0;
    FILE* logread = 0;
    FILE* logwrite = 0;
    if(argc > 1)
    {
        //Checking on argument
        int arg = 0;
        sscanf(argv[2], "%d", &arg);
       
        CreateGeneralFIFO();
        
        //Checking on Reader or Writer
        switch(arg)
        {
            case 1: //Writer
                {

                    logwrite = fopen("logwrite", "w+");
                    pFile = fopen( argv[1], "rb");
                    if(pFile == 0)
                    {
                        perror("1. Opening file");
                    }

                    fprintf(logwrite, "1. This proccess is writer, waiting Reader");
                    
                    fd_GF = open("GeneralFIFO", O_RDWR);
                    Writer(&fd_GF, pFile, logwrite);
                    break;
                }
            case 2: //Reader
                {
                    logread = fopen("logread", "w+");
                    perror("Open");
                    fprintf(logread, "1. This process is reader, waiting Writer");

                    fd_GF = open("GeneralFIFO", O_WRONLY);
                    Reader(&fd_GF, logread);
                    break;
                }
            default:
                {
                    printf("\nPlease choose 1 for writer or 2 for reader");
                    break;
                }		
        }
    }
    else
    {
        printf("\nNo arguments");
    }
    return 0;
}

int Reader(int* fd_GF, FILE* logread)
{  
    //Know pid of the process
    pid_t pid = getpid();
    fprintf(logread, "\n2. The pid of the pr.Writer = %d", pid);

    //Transform pid to char
    char c_pid[10] = {};
    sprintf(c_pid, "%d", pid); 
    fprintf(logread, "\n3. Transform pid to string_%s_", c_pid);

    //Make FIFO with name of own PID
    if(mkfifo(c_pid, 0666) == 0)
    {
         fprintf(logread, "\n4. Pers FIFO was made");
    }
    else
    {
         perror("Error, Personal mkfifo not success");
    }

    //Opening Pers. FIFO 
    int fd_pr = 0;

// <CR.SECT start>
    fd_pr = open(c_pid, O_NONBLOCK | O_RDONLY);
  
    if(fd_pr == -1) 
    {
        perror("Open PF, error");
    }
    else
    {
        fprintf(logread, "\n5. Openig PF correct, fd_PersFIFO = %d", fd_pr);
    }
   
    //Write to General FIFO own pid
    int numb_bytes = 0;

    numb_bytes = write(*fd_GF, c_pid, 10); 
    
    if(numb_bytes == -1)
    {
        perror("Write pid in GF");
    }
    else
    {
        fprintf(logread, "\n6. Pid of proccess was written in GenFIFO, number of bytes = %d", numb_bytes);
    }
    
    //Start process of reading from personal FIFO
    fprintf(logread, "\n7. Process reading almost started\n");
  
    fflush(stdout);
    char buffer[SIZE] = {};
    int endofread = 1;
    int i = 0;

  struct pollfd fds;
    fds.fd = fd_pr;
    fds.events = POLLIN;
    int timeout = 10000;
    
    int ret = poll(&fds, 1, timeout);

    if(ret <= 0)
    {
        fprintf(logread, "\n |ret = %d| ", ret);
        fprintf(logread, "\nRomeo has died, Juellete must kill herself \n");
        fflush(stdout);
        raise(SIGKILL); // Suicide!! WOOO-HOOO!!!!
    }
   
 // <CR.SECT ends>
    fcntl(fd_pr, F_SETFL, O_RDONLY);

    while(endofread > 0)
    {
        endofread = read(fd_pr, buffer, SIZE);
      
        if(endofread == -1)
        {   
            perror("Error, while reading");
        }
        else if(endofread == 0)
        {
            fprintf(logread, "\nENDOFREAD\n");
        }
       
        write(STDOUT_FILENO, buffer, endofread);
      
       // printf(" %d. endofr = %d \n", i, endofread);
        i++;
    }
    unlink(c_pid);
    return 0;
}

int Writer(int* fd_GF, FILE* pFile, FILE* logwrite)
{ 
   //Read from General FIFO pid from writer
    char c_pid[10] = {};
    int wasread = 0;
   
// <CR.SECT starts>
    wasread = read(*fd_GF, c_pid, 10);
   
    if (wasread == -1)
    {
        perror("Error, read pid from GF");
    }
    else
    {
        fprintf(logwrite, "\n2. The pid from GF was read ");
        fprintf(logwrite, "{c_pid = '%s'}", c_pid);
        fflush(stdout);
    }

    //sleep(1);

   //Open Personal FIFO and find FD_PR
    int fd_pr = 0;

    fd_pr = open(c_pid, O_NONBLOCK | O_WRONLY);
   
    if(fd_pr == -1)
    {
         perror("Error, Open PF from proccess");
    }
    else
    {
       fprintf(logwrite, "\n3. Opening PF was correct, fd_pr = %d", fd_pr);
    }

   //Writing to Pers.FIFO 
    int newnumb_bytes = 1;
    char bf[SIZE] = {};
    int i = 1;
   
    fprintf(logwrite, "\n4. The process of writing almost started");
    fflush(stdout);
   
// <CR.SECT ends>
    fcntl(fd_pr, F_SETFL, O_WRONLY);
   
    while(newnumb_bytes > 0)
    {
        ssize_t t = read(fileno(pFile), bf, SIZE);
       
        newnumb_bytes = write(fd_pr, bf, t);
       
        fprintf(logwrite, "\n   %d. numb_bytes_wasread = %d", i, newnumb_bytes);
        i++;
    }

    fprintf(logwrite, "\n5. Done\n");
    unlink(c_pid);
    return 0;
}

int CreateGeneralFIFO()
{		
    //If General Fifo is not created
    if(mkfifo("GeneralFIFO", 0666) == 0)
    {
        printf("0. GeneralFIFO was created");
    }
    else
    {
        perror("0. mkfifo_GF");
    }
    return 0;
}




   /* struct pollfd fds[1];
    fds[0].fd = fd_pr;
    fds[0].events = POLLOUT;
    int timeout = 10000;
    int ret = poll(fds, 1, timeout);
    timeout = 10;

    if(ret <= 0 || (fds[0].revents & POLLOUT) == 0)
    {
        printf(" |ret = %d| ", ret);
        if(unlink(c_pid) == -1)
            perror("Error with unlink!\n");
        perror("Romeo has died, Juellete must kill herself");
 exit(11);       
    }*/

//exit(9);
   

