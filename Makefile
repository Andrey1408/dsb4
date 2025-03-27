all:
	clang-3.5  -std=c99 -Wall -pedantic -Werror *.c -o main -L. -lruntime

wsl: 
	clang-14 -std=c99 -Wall -g -O0 -pedantic -Werror *.c -o main -L. -lruntime

run-mutexl: 
	LD_PRELOAD=./libruntime.so ./main -p 9 --mutexl

run:
	LD_PRELOAD=./libruntime.so ./main -p 9 

clean:
	rm -f main
	rm -f *.log