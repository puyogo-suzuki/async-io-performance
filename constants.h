#define BLK_SIZE 1024
#define TEST_MODE 0
#define TEST_CHAR 'a'
#if TEST_MODE
#define ITERATION 1
#else
#define ITERATION 100
#endif

#define FAILCHECK(pred, fname) if(pred) { \
fprintf(stderr, fname" fail.\n");\
exit(EXIT_FAILURE);\
}