all:
	clang-3.5  -std=c99 -Wall -pedantic -Werror *.c -o main -L. -lruntime

run: all 
	LD_PRELOAD=./libruntime.so ./main -p 3 10 20 30

clean:
	rm -f main.c
	rm -f *.log