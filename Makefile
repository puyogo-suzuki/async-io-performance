CC = clang
CFLAGS = 

sync-unix : sync-unix.c
	$(CC) $(CFLAGS) sync-unix.c -o sync-unix.out

async-unix : async-unix.c
	$(CC) $(CFLAGS) async-unix.c -o async-unix.out -lrt

async-unix2 : async-unix2.c
	$(CC) $(CFLAGS) async-unix2.c -o async-unix2.out -lrt

async-unix3 : async-unix3.c
	$(CC) $(CFLAGS) async-unix3.c -o async-unix3.out -lrt

async-linux : async-linux.c
	$(CC) $(CFLAGS) async-linux.c -o async-linux.out 

initfile.out : initfile.c
	$(CC) $(CFLAGS) initfile.c -o initfile.out

all : initfile.out