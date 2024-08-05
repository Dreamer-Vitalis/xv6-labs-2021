// sleep.c

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(2, "Usage: sleep [ticks num]\n");
        exit(1);
    }
    int sleep_time = atoi(argv[1]);
    int ret = sleep(sleep_time);
    exit(ret);
}
