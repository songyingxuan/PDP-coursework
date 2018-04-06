SRC = code.c ran2.c pool.c squirrel-functions.c
LFLAGS=-lm
CC=mpicc

all: 
	$(CC) -o code $(SRC) $(LFLAGS)

clean:
	rm -f code
