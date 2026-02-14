CC = gcc
CFLAGS = -Wall -Wextra
LIBS = -lreadline -lm

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)

    BREW_PREFIX := $(shell brew --prefix 2>/dev/null || echo "/opt/homebrew")
    
    CFLAGS += -I$(BREW_PREFIX)/opt/readline/include
    LDFLAGS += -L$(BREW_PREFIX)/opt/readline/lib
endif

all: lisp

debug: CFLAGS += -g -DDEBUG
debug: lisp

lisp: interpreter.c mpc.c mpc.h
	$(CC) $(CFLAGS) $(LDFLAGS) interpreter.c mpc.c -o interpreter $(LIBS)

clean:
	rm -f interpreter