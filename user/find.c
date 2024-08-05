#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char *path, char *file)
{
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
        fprintf(2, "find: %s is not a directory\n", path);
        close(fd);
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

        /*
        de的结构：
        struct dirent {
        ushort inum;
        char name[DIRSIZ]; 【文件名】
        };
        */
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if (de.inum == 0)
                continue;
            if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;
            // 这行代码执行后，buf就是目前处理的文件/文件夹的完整路径（含名称），而p就是文件的名称
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            // 把文件的状态信息保存到st中，如果获取stat状态失败，则报错并continue
            /*
            st的结构：
            struct stat {
            int dev;     // File system's disk device
            uint ino;    // Inode number
            short type;  // Type of file 【文件类型】
            short nlink; // Number of links to file
            uint64 size; // Size of file in bytes
            };
            */
            if (stat(buf, &st) < 0)
            {
                printf("find: cannot stat %s\n", buf);
                continue;
            }
            if (st.type == T_DIR)
            {
                find(buf, file);
                continue;
            }
            else if(st.type == T_FILE && !strcmp(de.name, file))
                printf("%s\n", buf);
        }
        break;
    }
    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(2, "Usage: find PathName FileName\n");
        exit(1);
    }
    find(argv[1], argv[2]);
    exit(0);
}
