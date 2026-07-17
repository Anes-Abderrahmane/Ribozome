CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pedantic -O2 -MMD -MP
SRC = main.c lexer.c parser.c codegen.c
OBJ = $(SRC:.c=.o)
TARGET = main
TRANSLATOR_SRC = translator.c
TRANSLATOR_OBJ = $(TRANSLATOR_SRC:.c=.o)
TRANSLATOR = translator

all: $(TARGET) $(TRANSLATOR)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

$(TRANSLATOR): $(TRANSLATOR_OBJ)
	$(CC) $(CFLAGS) -o $(TRANSLATOR) $(TRANSLATOR_OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

-include $(OBJ:.o=.d)
-include $(TRANSLATOR_OBJ:.o=.d)

test: $(TARGET) $(TRANSLATOR)
	./$(TARGET) sample.dna
	./output.protein

clean:
	rm -f $(TARGET) $(TRANSLATOR) $(OBJ) $(TRANSLATOR_OBJ) $(OBJ:.o=.d) $(TRANSLATOR_OBJ:.o=.d)

.PHONY: all test clean
