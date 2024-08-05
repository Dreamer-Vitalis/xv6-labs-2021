// pingpong.c

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if (argc != 1)
    {
        fprintf(2, "Usage: pingpong\n");
        exit(1);
    }
    int pipefd1[2], pipefd2[2];
    pipe(pipefd1), pipe(pipefd2);
    char buf[10];
    if(fork() == 0)
    {
        // 子进程
        close(pipefd1[1]);
        close(pipefd2[0]);

        read(pipefd1[0], buf, strlen("ping"));
        printf("%d: received %s\n", getpid(), buf);
        write(pipefd2[1], "pong", strlen("pong"));

        close(pipefd1[0]);
        close(pipefd2[1]);
    }
    else
    {
        // 父进程
        close(pipefd1[0]);
        close(pipefd2[1]);

        write(pipefd1[1], "ping", strlen("ping"));
        //wait(0);
        read(pipefd2[0], buf, strlen("pong"));
        printf("%d: received %s\n", getpid(), buf);

        close(pipefd1[1]);
        close(pipefd2[0]);
    }
    exit(0);
}
