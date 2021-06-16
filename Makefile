# Makefile
FLAGS = -std=c99 -Wall -O1

shell: shell.c
	gcc ${FLAGS} -o shell shell.c

clean:
	rm -f shell