CC = gcc
CFLAGS = -Wall -Werror -pedantic -std=gnu18
LOGIN = chuan-chi
SUBMITPATH = ~cs537-1/handin/$(LOGIN)/P3
TARGET = wsh
TAR_FILE = $(LOGIN).tar.gz

.PHONY: all
all: $(TARGET)

$(TARGET): wsh.c wsh.h
	$(CC) $(CFLAGS) $^ -o $@

run: $(TARGET)
	./$(TARGET)

pack:
	tar -czvf $(TAR_FILE) wsh.h wsh.c Makefile README.md

submit: pack
	cp $(TAR_FILE) $(SUBMITPATH)

clean:
	rm -f $(TARGET) $(TAR_FILE)
