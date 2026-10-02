CC = gcc
CFLAGS = -Wall -Wextra -O2 -g

OBJS = coro.o switch_arm64.o

all: demo chat

libcoro.a: $(OBJS)
	ar rcs libcoro.a $(OBJS)

demo: main.o libcoro.a
	$(CC) $(CFLAGS) main.o -L. -lcoro -o demo

chat: chat.o libcoro.a
	$(CC) $(CFLAGS) chat.o -L. -lcoro -o chat

coro.o: coro.c coro.h
	$(CC) $(CFLAGS) -c coro.c -o coro.o

switch_arm64.o: switch_arm64.s
	$(CC) $(CFLAGS) -c switch_arm64.s -o switch_arm64.o

main.o: main.c coro.h
	$(CC) $(CFLAGS) -c main.c -o main.o

chat.o: chat.c coro.h
	$(CC) $(CFLAGS) -c chat.c -o chat.o

run: demo
	./demo

run_chat: chat
	./chat

clean:
	rm -f *.o *.a demo chat
