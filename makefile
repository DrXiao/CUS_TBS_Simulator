SRC := src
INCLUDE := include
CC := gcc
FLAG := -I$(INCLUDE) -lm
EXEC := main

build:
	$(CC) -g -o $(EXEC) $(SRC)/* $(FLAG)

clean:
	rm $(EXEC)
