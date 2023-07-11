#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "constants.h"

#include <sys/syscall.h>
#include <linux/aio_abi.h>

int io_setup(unsigned nr, aio_context_t *ctxp) {
	return syscall(__NR_io_setup, nr, ctxp);
}

int io_destroy(aio_context_t ctx) {
	return syscall(__NR_io_destroy, ctx);
}

int io_submit(aio_context_t ctx, long nr, struct iocb **iocbpp) {
	return syscall(__NR_io_submit, ctx, nr, iocbpp);
}

int io_getevents(aio_context_t ctx, long min_nr, long max_nr,
		struct io_event *events, struct timespec *timeout) {
	return syscall(__NR_io_getevents, ctx, min_nr, max_nr, events, timeout);
}


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
    FAILCHECK(stat(argv[1], &sb) != 0, "stat")
    run(argv[1], sb.st_size);
    return 0;
}

struct my_context_t {
    struct iocb cb;
    int data;
    struct iocb * cbs[1];
    char buf[BLK_SIZE];
};

void setup_my_context(struct my_context_t * ctx, const int fd, const int data) {
    ctx->cb.aio_data = data;
    ctx->cb.aio_fildes = fd;
    ctx->cb.aio_lio_opcode = IOCB_CMD_PREAD;
    ctx->cb.aio_offset = 0;
    ctx->cb.aio_nbytes = BLK_SIZE;
    ctx->cb.aio_buf = (__u64)ctx->buf;
    ctx->data = data;
    ctx->cbs[0] = &ctx->cb; // io_submit用
}


void run(char * filename, int filesize) {
    int fd = open(filename, O_RDWR);
    aio_context_t ctx = {0};
    struct my_context_t ctxs[2] = {0};
    struct my_context_t * ctx1 = ctxs;
    struct my_context_t * ctx2 = &ctxs[1];
    struct io_event events[2];
    int blk_size_i = BLK_SIZE / sizeof(int);
    int size = filesize / BLK_SIZE;
    FAILCHECK(io_setup(128, &ctx) < 0, "io_setup")
    setup_my_context(ctx1, fd, 1);
    setup_my_context(ctx2, fd, 2);
    for(int iterate = 0; iterate < ITERATION; ++iterate) {
        ctx1->cb.aio_offset = 0;
        ctx1->cb.aio_lio_opcode = IOCB_CMD_PREAD;
        ctx2->cb.aio_offset = BLK_SIZE;
        ctx2->cb.aio_lio_opcode = IOCB_CMD_PREAD;
        // 最初の読み込み
        FAILCHECK(io_submit(ctx, 1, ctx1->cbs) != 1, "io_submit")
        FAILCHECK(io_submit(ctx, 1, ctx2->cbs) != 1, "io_submit")
        int next_ok = 0;
        for(int i = 0; i < size; ++i) {
            int loop = next_ok == 0;
            // next_okが立っていた場合，io_geteventsで何もイベントが得られなくても良い．
            // なぜなら，i番目の読み込みは前のイテレーションで終わっているから．
            next_ok = 0;
            do {
                int evnum = io_getevents(ctx, loop == 1 ? 1 : 0, 2, events, NULL);
                int isCtx1Finished = 0, isCtx2Finished = 0;
                int ctx1Res = 0, ctx2Res = 0;
                FAILCHECK(evnum == 0, "io_getevents")
                for(int ev = 0 ; ev < evnum; ev++) {
                    if(events[ev].data == ctx1->data) {
                        isCtx1Finished = 1;
                        ctx1Res = events[ev].res;
                    }
                    if(events[ev].data == ctx2->data) {
                        isCtx2Finished = 1;
                        ctx2Res = events[ev].res;
                    }
                }
                if(isCtx1Finished) {
                    if(ctx1->cb.aio_lio_opcode == IOCB_CMD_PWRITE) { // 読み込みまだ
                        FAILCHECK(ctx1Res == 0, "write")
                        ctx1->cb.aio_lio_opcode = IOCB_CMD_PREAD;
                        ctx1->cb.aio_offset = BLK_SIZE * i;
                        FAILCHECK(io_submit(ctx, 1, ctx1->cbs) != 1, "io_submit")
                    } else {// 読み込み完了!
                        FAILCHECK(ctx1Res == 0, "read")
                        loop = 0;
                    }
                }
                if(isCtx2Finished) {
                    if(ctx2->cb.aio_lio_opcode == IOCB_CMD_PWRITE) { // 書き込みが終わった！
                        FAILCHECK(ctx2Res == 0, "write")
                        ctx2->cb.aio_lio_opcode = IOCB_CMD_PREAD; // 先行読み込み
                        ctx2->cb.aio_offset = BLK_SIZE * (i+1);
                        if(size > i+1) FAILCHECK(io_submit(ctx, 1, ctx1->cbs) != 1, "io_submit")
                    } else {
                        FAILCHECK(ctx2Res == 0, "read")
                        next_ok = 1;
                    }
                }
            } while(loop);
            int * buf = (int *) ctx1->buf;
            for(int j = 0; j < blk_size_i; ++j)
                #if TEST_MODE
                buf[j] = val;
                #else
                buf[j] = ((int *)buf)[j] * val;
                #endif
            ctx1->cb.aio_lio_opcode = IOCB_CMD_PWRITE;
            FAILCHECK(io_submit(ctx, 1, ctx1->cbs) != 1, "io_submit")

            struct my_context_t * tmp = ctx1;
            ctx1 = ctx2;
            ctx2 = tmp;
        }
    }
    close(fd);
}