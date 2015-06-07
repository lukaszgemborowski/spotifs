.PHONY: all
.PHONY: clean

SRC=$(wildcard src/*.c)
OBJ=$(SRC:.c=.o)

INCLUDE_PATH=-I./libspotify-12.1.51-Linux-x86_64-release/include
LIBRARY_PATH=-L./libspotify-12.1.51-Linux-x86_64-release/lib
LIBRARIES=-lspotify `pkg-config fuse glib-2.0 --libs`
DEFINES=-D_FILE_OFFSET_BITS=64

all : spotifs

clean:
	rm -f $(OBJ) spotifs

spotifs: $(OBJ)
	gcc -ggdb -o $@ $^ $(LIBRARY_PATH) $(LIBRARIES)

%.o: %.c
	gcc `pkg-config glib-2.0 --cflags` -ggdb -Wall -Wformat -c $< -o $@ $(DEFINES) $(INCLUDE_PATH)
