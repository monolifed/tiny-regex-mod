all: match

match: tre.h
match: match.c
	gcc -O2 -Wall -Wextra -pedantic -std=c99 -o $@ match.c

clean:
	rm -f match
