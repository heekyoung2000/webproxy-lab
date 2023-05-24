CC = gcc
CFLAGS = -O2 -Wall -I ..

all: adder form-adder head-adder

adder: adder.c
	$(CC) $(CFLAGS) -o adder adder.c

form-adder : form-adder.c
	$(CC) $(CFLAGS) -o form-adder form-adder.c

head-adder : head-adder.c
	$(CC) $(CFLAGS) -o head-adder head-adder.c

clean:
	rm -f form-adder head-adder adder *~
