CC = gcc
CFLAGS = -Wall -Wextra -O2

OBJ = main.o Generation.o

programme: $(OBJ)
    $(CC) $(CFLAGS) -o programme $(OBJ)

main.o: main.c Generation.h
    $(CC) $(CFLAGS) -c main.c

Generation.o: Generation.c Generation.h
    $(CC) $(CFLAGS) -c Generation.c

clean:
    rm -f *.o programme
