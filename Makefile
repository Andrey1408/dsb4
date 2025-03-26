all:
	clang-3.5  -std=c99 -Wall -pedantic -Werror *.c -o main -L. -lruntime

wsl: 
	clang-14 -std=c99 -Wall -g -O0 -pedantic -Werror *.c -o main -L. -lruntime

run: 
	LD_PRELOAD=./libruntime.so ./main -p 5 --mutexl

clean:
	rm -f main
	rm -f *.log