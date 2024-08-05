#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char *argv[])
{
    char *new_argv[MAXARG];
    for (int i = 1; i < argc; i++)
        new_argv[i - 1] = argv[i];

    if (fork() == 0)
    {
        char c;
        int p = argc - 1; // pointer，指当前处理的参数的位置
        char buf[1024]; // 所有参数都放在这里
        int t = 0; // t表示当前处理的参数开始的位置；例如：处理echo hello world，在开始处理hello时，t = 0， 在开始处理world时, t =  6
        while (read(0, &c, sizeof(char)) == sizeof(char))
        {
            int tt = t; // tt表示当前处理的字符串的位置，例如：处理echo hello world，在处理最后的'd'时 tt = 11
            do
            {
                if(c != ' ' && c != '\n')
                    buf[tt++] = c;
                else
                    break;
            } while (read(0, &c, sizeof(char)) == sizeof(char));
            // 原始的丑陋版本
            /*
            while (c != ' ' && c != '\n')
            {
                buf[tt++] = c;
                if (read(0, &c, sizeof(char)) != sizeof(char))
                    break;
                
            }
            */
            buf[tt] = '\0';  // 添加字符串结束标志
            if (c == ' ')
            {
                new_argv[p++] = buf + t;
                // 到下一次循环时，t从'\0'的下一个位置开始，echo hello world
                // 在处理完hello之后会变成【'h''e''l''l''o''\0'】，下一次是【'w'...】，t的位置要赋值为下一次开始处理world的'w'的位置
                t = tt + 1;
            }
            else if (c == '\n')
            {
                new_argv[p++] = buf + t;
                new_argv[p] = 0; // 赋值为NULL，避免因为存在多条命令而出错（因为后面的参数可能是上一条命令的，然后就会出错）
                if (fork() == 0)
                    exec(new_argv[0], new_argv);
                else
                    wait(0);
                p = argc - 1; // 处理完'\n'，下一次开始时是全新的命令，要重新定位p
                t = tt + 1;
            }
        }
    }
    else
        wait(0);

    exit(0);
}