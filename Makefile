CC = clang
CFLAGS = 

initfile.out : initfile.c
	$(CC) $(CFLAGS) initfile.c -o initfile.out

all : initfile.out