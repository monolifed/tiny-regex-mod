CFLAGS:=-O2 -Wall -Wextra -Wvla -Wsign-conversion -pedantic -std=c99
APPNAME:=match

ifeq ($(OS),Windows_NT)
	APPNAME:=$(APPNAME).exe
	CC:=gcc
	RM:=del /Q
endif

all: match

match: match.c tre.h
	$(CC) $(CFLAGS) -o $@ $<

clean:
	$(RM) $(APPNAME)
