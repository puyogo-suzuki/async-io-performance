#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "constants.h"

// UNIX Asynchronous I/O header
#include <aio.h>

int val;
int filesize;

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
    FAILCHECK(stat(argv[1], &sb) != 0, "stat")
    run(argv[1], sb.st_size);
    return 0;
}
/*
    time -->
    block0: | read(0) | calc(0) | write(0)    |
    block1:           || read(1) || calc(1)   || write(1)   |
    block2:                                    ||  read(0)  || calc(0)    || write(0)   |
    block3:                                                                 ||  read(1)  || calc(1)    || write(1)   |

 */

void run(char * filename, int filesize) {
    int fd = open(filename, O_RDWR);
    struct aiocb cbs[2] = {0};
    struct aiocb * cb_r = cbs;
    struct aiocb * cb_w = &(cbs[1]);
    // aio_suspend用にaiocbの配列を用意する．
    const struct aiocb * const cb_r_list[] = {cb_r};
    const struct aiocb * const cb_w_list[] = {cb_w};
    char _buf[BLK_SIZE * 2];
    int * buf = (int *)_buf;   // 読み込みバッファと書き込みバッファを用意する．
    int * buf2 = (int *)(&(_buf[BLK_SIZE])); // 1回計算するごとに入れ替わる
    int blk_size_i = BLK_SIZE / sizeof(int);
    int size = filesize / BLK_SIZE;

    // aiocbの初期化
    cb_r->aio_fildes = fd; cb_w->aio_fildes = fd;  // 読み書きするファイル記述子
    cb_r->aio_nbytes = BLK_SIZE; cb_w->aio_nbytes = BLK_SIZE;  // 読み書きするサイズ
    cb_r->aio_lio_opcode = LIO_NOP; cb_w->aio_lio_opcode = LIO_NOP; // lio_listio関数用のフィールド．なんとなくLIO_NOP（何もしない）入れとく
    cb_r->aio_sigevent.sigev_notify = SIGEV_NONE; cb_w->aio_sigevent.sigev_notify = SIGEV_NONE;// シグナルは発生させない．
    for(int iterate = 0; iterate < ITERATION; ++iterate) {
        // 最初の read
        cb_r->aio_offset = 0;
        cb_r->aio_buf = buf;
        cb_r->aio_nbytes = BLK_SIZE;
        aio_read(cb_r);
        for(int i = 0; i < size; ++i) {
            // 読み込みが終わるまで待つ
            aio_suspendloop:
            if(aio_suspend(cb_r_list, 1, NULL) == -1){
                if(errno == EINTR) goto aio_suspendloop; // シグナルによって中断されたので，また待つ．
                fprintf(stderr, "read fail.\n");
                exit(EXIT_FAILURE);
            }
            FAILCHECK(aio_return(cb_r) == 0, "read") // read()関数呼んだ時の戻り値と同じ戻り値．
            cb_r->aio_offset = (i + 1) * BLK_SIZE;
            cb_r->aio_buf = buf2;
            if(i + 1 < size) aio_read(cb_r);
            for(int j = 0; j < blk_size_i; ++j)
                #if TEST_MODE
                buf[j] = val;
                #else
                buf[j] = ((int *)buf)[j] * val;
                #endif

            // 書き込みが終わるまで待つ
            aio_suspendloop2:
            if(i > 0) {
                if(aio_suspend(cb_w_list, 1, NULL) == -1){
                    if(errno == EINTR) goto aio_suspendloop2; // シグナルによって中断されたので，また待つ．
                    fprintf(stderr, "write fail.\n");
                    exit(EXIT_FAILURE);
                }
                FAILCHECK(aio_return(cb_w) == 0, "write")
            }

            cb_w->aio_offset = i * BLK_SIZE;
            cb_w->aio_buf = buf;
            aio_write(cb_w);

            int * tmp = buf;
            buf = buf2;
            buf2 = tmp;
        }
        aio_suspendloop3:
        if(aio_suspend(cb_w_list, 1, NULL) == -1){
            if(errno == EINTR) goto aio_suspendloop3;
        }
        FAILCHECK(aio_return(cb_w) == 0, "write")
    }
    close(fd);
}