#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void receive_nums(int fd[])
{
    close(fd[1]);

    int now, tmp;
    read(fd[0], &now, sizeof(int));
    printf("prime %d\n", now);

    // 终止条件：没有数据可以Read出来了
    if(read(fd[0], &tmp, sizeof(int)) == 0)
        return;
    // 如果没有触发终止条件，则创建子进程，继续递归执行
    int new_fd[2];
    pipe(new_fd);
    
    if (fork() == 0) // 子进程
    {
        close(fd[0]);
        receive_nums(new_fd);
    }
    else // 父进程
    {
        close(new_fd[0]);

        do
        {
            if (tmp % now != 0) // 可能是素数，写入
                write(new_fd[1], &tmp, sizeof(int));
        } while (read(fd[0], &tmp, sizeof(int)) != 0);
        
        close(fd[0]);
        close(new_fd[1]);
        wait(0);
    }
}

int main(int argc, char *argv[])
{
    int fd[2];
    pipe(fd);

    if (fork() == 0) // 子进程
    {
        receive_nums(fd);
    }
    else // 父进程
    {
        close(fd[0]);
        for (int i = 2; i <= 35; i++)
            write(fd[1], &i, sizeof(int));
        close(fd[1]);
        wait(0);
    }
    
    exit(0);
}
