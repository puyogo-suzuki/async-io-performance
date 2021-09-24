#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "constants.h"

int val;

void run(char * filename, int filesize);

int main(int argc, char * argv[]) {
    srand(1);
    if(argc != 2) {
        printf("Usage: %s <file name>", argv[0]);
        return 0;
    }
    #if TEST_MODE
    val = (TEST_CHAR << 24) | (TEST_CHAR << 16) | (TEST_CHAR << 8) | TEST_CHAR;
    #else
    val = rand();
    #endif
    struct stat sb;
    if(stat(argv[1], &sb) != 0) {
        fprintf(stderr, "stat fail.\n");
        exit(EXIT_FAILURE);
    }
    run(argv[1], sb.st_size);
    return 0;
}

void run(char * filename, int filesize) {
    int fd = open(filename, O_RDWR);
    char _buf[BLK_SIZE];
    int * buf = (int *)_buf;
    int blk_size_i = BLK_SIZE / sizeof(int); // 1ブロック当たりのintの数
    int size = filesize / BLK_SIZE; // 何回に分けて書き込むか
    for(int iterate = 0; iterate < ITERATION; ++iterate) {
        for(int i = 0; i < size; ++i) {
            // 読み込む
            if(read(fd, buf, sizeof(char) * BLK_SIZE) == 0) {
                fprintf(stderr, "read fail.\n");
                exit(EXIT_FAILURE);
            }

            for(int j = 0; j < blk_size_i; ++j)
                #if TEST_MODE
                buf[j] = val;
                #else
                buf[j] = ((int *)buf)[j] * val;
                #endif

            // 最初のreadでBLK_SIZEぶん読み書きの位置が変わってしまったので，BLK_SIZE戻す．
            lseek(fd, -BLK_SIZE, SEEK_CUR);
            // 書き込む
            if(write(fd, buf, sizeof(char) * BLK_SIZE) == 0) {
                fprintf(stderr, "write fail.\n");
                exit(EXIT_FAILURE);
            }
        }
        // 最初に戻る．
        lseek(fd, 0, SEEK_SET);
    }
    close(fd);
}