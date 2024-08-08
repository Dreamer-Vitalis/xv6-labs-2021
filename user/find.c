#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char *
fmtname(char *path)
{
    static char buf[DIRSIZ + 1];
    char *p;

    // Find first character after last slash.
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;

    // Return blank-padded name.
    if (strlen(p) >= DIRSIZ)
        return p;
    memmove(buf, p, strlen(p));
    memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
    return buf;
}

void find(char *path, char *file)
{
    /*
    1、stat/fstat
    使用：
        fstat(fd, &st)  参数是文件描述符
        stat(buf, &st)  参数是路径名
    作用：读取 给的路径名/文件描述符 是什么类型（文件/文件夹）：
        如：给定./a/b，b可能是文件，也可能是文件夹
        保存的信息存储在st中（struct stat st;）

        struct stat {
            int dev;     // File system's disk device
            uint ino;    // Inode number
            short type;  // Type of file 【文件类型】
            short nlink; // Number of links to file
            uint64 size; // Size of file in bytes
        };
         
    2、struct dirent de;
    对应：read(fd, &de, sizeof(de)) == sizeof(de)
    作用：读取一个目录下的目录项，
        如：./a/b，b是目录，那么就读取b目录下的所有目录项（每个目录项对应一个文件/文件夹）

        struct dirent {
            ushort inum;
            char name[DIRSIZ]; 【文件名】
        };
    */

    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type)
    {
    case T_FILE:
        fprintf(2, "find: %s is not a directory", path);
        break;

    case T_DIR:
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)
        {
            printf("find: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if (de.inum == 0)
                continue;
            if(!strcmp(de.name, ".") || !strcmp(de.name, ".."))
                continue;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if (stat(buf, &st) < 0)
            {
                printf("find: cannot stat %s\n", buf);
                continue;
            }

            switch(st.type)
            {
                case T_FILE:
                    if(!strcmp(de.name, file))
                        printf("%s\n", buf);
                    break;
                case T_DIR: // 递归查找下去
                    find(buf, file);
                    break;
            }
        }
        break;
    }
    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc != 2 && argc != 3)
    {
        fprintf(2, "Usage: find [Path] FileName\n");
        exit(1);
    }
    if (argc == 2)
        find(".", argv[1]);
    else
        find(argv[1], argv[2]);
    exit(0);
}
