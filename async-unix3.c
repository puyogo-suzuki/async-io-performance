/*
  WONT MOVE?
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "constants.h"
#include <signal.h>

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

struct context_t {
    struct aiocb cb_r;
    struct aiocb cb_w;
    int i;
    int size;
    char buf[BLK_SIZE];
};

void writeHandler(union sigval si) {
    struct context_t * ctx = (struct context_t *)si.sival_ptr;
    FAILCHECK(aio_return(&(ctx->cb_w)) == 0, "write")
    ctx->cb_r.aio_offset = ctx->i * BLK_SIZE;
    if(ctx->i < ctx->size) {
        aio_read(&(ctx->cb_r));
    }
}

void initcontext(struct context_t * ctx, const int fd, const int size) {
    ctx->cb_r.aio_fildes = fd; ctx->cb_w.aio_fildes = fd;
    ctx->cb_r.aio_nbytes = BLK_SIZE; ctx->cb_w.aio_nbytes = BLK_SIZE;
    ctx->cb_r.aio_lio_opcode = LIO_NOP; ctx->cb_w.aio_lio_opcode = LIO_NOP;
    ctx->cb_r.aio_sigevent.sigev_notify = SIGEV_NONE;
    ctx->cb_w.aio_sigevent.sigev_notify = SIGEV_THREAD;
    ctx->cb_w.aio_sigevent.sigev_notify_function = writeHandler;
    ctx->cb_w.aio_sigevent.sigev_notify_attributes = NULL;
    ctx->cb_w.aio_sigevent.sigev_value.sival_ptr = ctx;
    ctx->cb_r.aio_buf = ctx->buf; ctx->cb_w.aio_buf = ctx->buf;
    ctx->size = size;
}

void waitRead(struct context_t * ctx) {
    const struct aiocb * const cb_r_list[] = {&(ctx->cb_r)};
    const struct aiocb * const cb_w_list[] = {&(ctx->cb_w)};

    start:
    if(aio_suspend(cb_w_list, 1, NULL) == -1){
        if(errno == EINTR) goto start; // シグナルによって中断されたので，また待つ．
        fprintf(stderr, "write suspend fail.\n");
        exit(EXIT_FAILURE);
    }
    start2:
    if(aio_suspend(cb_r_list, 1, NULL) == -1){
        if(errno == EINTR) goto start2; // シグナルによって中断されたので，また待つ．
        fprintf(stderr, "read fail.\n");
        exit(EXIT_FAILURE);
    }
    FAILCHECK(aio_return(&(ctx->cb_r)) == 0, "read")
}

/*
    time -->
    block0: | read(0) | calc(0) | write(0)    |
    block1:           || read(1) || calc(1)   || write(1)   |
    block2:                                   ||  read(0)  || calc(0)    || write(0)   |
    block3:                                                ||  read(1)  || calc(1)    || write(1)   |

 */

void run(char * filename, int filesize) {
    int fd = open(filename, O_RDWR);

    struct context_t * ctxs = calloc(2, sizeof(struct context_t));
    struct context_t * ctx0 = ctxs;
    struct context_t * ctx1 = &ctxs[1];

    int blk_size_i = BLK_SIZE / sizeof(int);
    int size = filesize / BLK_SIZE;

    initcontext(ctx0, fd, size);
    initcontext(ctx1, fd, size);

    for(int iterate = 0; iterate < ITERATION; ++iterate) {
        // 最初の read
        ctx0->cb_r.aio_offset = 0;
        aio_read(&(ctx0->cb_r));
        ctx1->cb_r.aio_offset = BLK_SIZE;
        if(size > 1)
            aio_read(&(ctx1->cb_r));
        for(int i = 0; i < size; ++i) {
            // 読み込みが終わるまで待つ
            waitRead(ctx0);
            int * buf = (int *)ctx0->buf;
            for(int j = 0; j < blk_size_i; ++j)
                #if TEST_MODE
                buf[j] = val;
                #else
                buf[j] = ((int *)buf)[j] * val;
                #endif
            ctx0->i = i;
            ctx0->cb_w.aio_offset = i * BLK_SIZE;
            aio_write(&(ctx0->cb_w));

            struct context_t * tmp = ctx0;
            ctx0 = ctx1;
            ctx1 = tmp;
        }
        const struct aiocb * const cb_w_list[] = {&(ctx0->cb_w)};
        const struct aiocb * const cb_w_list2[] = {&(ctx1->cb_w)};
        aio_suspendloop:
        if(aio_suspend(cb_w_list, 1, NULL) == -1){
            if(errno == EINTR) goto aio_suspendloop;
        }
        aio_suspendloop1:
        if(size > 1 && aio_suspend(cb_w_list2, 1, NULL) == -1){
            if(errno == EINTR) goto aio_suspendloop1;
        }
        FAILCHECK(aio_return(&(ctx0->cb_w)) == 0, "write")
        if(size > 1) FAILCHECK(aio_return(&(ctx1->cb_w)) == 0, "write")
    }
    close(fd);
    free(ctxs);
}