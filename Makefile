
CC=gcc
# -O2
CCFLAGS=-g -Wall -Wextra -Waggregate-return -Wformat -Wshadow -Wconversion \
	-Wredundant-decls -Wpointer-arith -Wcast-align -pedantic -std=c11

CCFLAGS+=`pkg-config --cflags gtk+-3.0 gmodule-2.0`
LDFLAGS+=`pkg-config --libs gtk+-3.0 gmodule-2.0`

OBJS=pipeglade.o input-handlers.o event-handlers.o get-values.o \
	toaentry.o toaeditor.o gundo.o

pipeglade: $(OBJS)
	$(CC) $(CCFLAGS) -o pipeglade $(OBJS) $(LDFLAGS)

pipeglade.o: pipeglade.c pipeglade.h
	$(CC) $(CCFLAGS) -o pipeglade.o -c pipeglade.c

input-handlers.o: input-handlers.c pipeglade.h toaentry.h toaeditor.h gundo.h
	$(CC) $(CCFLAGS) -o input-handlers.o -c input-handlers.c

event-handlers.o: event-handlers.c pipeglade.h toaentry.h toaeditor.h gundo.h
	$(CC) $(CCFLAGS) -o event-handlers.o -c event-handlers.c

get-values.o: get-values.c pipeglade.h
	$(CC) $(CCFLAGS) -o get-values.o -c get-values.c

gundo.o: gundo.c gundo.h
	$(CC) $(CCFLAGS) -o gundo.o -c gundo.c

toaeditor.o: toaeditor.c toaeditor.h gundo.h
	$(CC) $(CCFLAGS) -o toaeditor.o -c toaeditor.c

toaentry.o: toaentry.c toaentry.h gundo.h
	$(CC) $(CCFLAGS) -o toaentry.o -c toaentry.c

clean:
	rm -f *.o *.fifo pipeglade
