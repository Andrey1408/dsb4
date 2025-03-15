all:
	clang-3.5  -std=c99 -Wall -pedantic -Werror *.c -o main -L. -lruntime

run: all
	LD_PRELOAD=./libruntime.so ./main -p 3 --mutexl

clean:
	rm -f main
	rm -f *.log:Wall