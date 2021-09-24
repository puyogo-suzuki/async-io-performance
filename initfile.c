#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char * argv[]) {
    srand(1);
    if(argc != 3) {
        printf("Usage: %s <file size> <file name>", argv[0]);
        return 0;
    }
    int size = atoi(argv[1]);
    int fd = creat(argv[2], 0777);
    char buf[1024];
    int i = 0;
    for(i = 0; i < size; i += 1024) {
        for(int j = 0; j < 1024; j += sizeof(int))
            *(int *)(&buf[j]) = rand();
        write(fd, buf, sizeof(buf));
    }
    if(i - size > 0) {
        int n = 1024 - (i - size);
        for(int j = 0; j < 1024; j += sizeof(int))
            *(int *)(&buf[j]) = rand();
        write(fd, buf, sizeof(char) * n);
    }
    close(fd);
    return 0;
}