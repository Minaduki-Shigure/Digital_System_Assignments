CC = gcc
CFLAGS = -Wall -g

all : myls mycp

myls : ./obj/myls.o ./obj/Fat12.o
	$(CC) $(CFLAGS) -o $@ $^

mycp : ./obj/mycp.o ./obj/Fat12.o
	$(CC) $(CFLAGS) -o $@ $^

./obj/%.o : ./src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf myls mycp ./obj/myls.o ./obj/mycp.o ./obj/Fat12.o