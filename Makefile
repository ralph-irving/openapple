#
#
#

CC = gcc

#profiling flag
#CFLAGS = -Wall -O3 -ggdb -march=i686 -pg
CFLAGS = -Wall -O3 -ggdb -march=i686 -fomit-frame-pointer

CPPFLAGS = `gtk-config --cflags` `sdl-config --cflags`
#LDFLAGS = -Wl,-rpath,/usr/local/lib
LIBS = `gtk-config --libs` `sdl-config --libs`

SRCS = apple.c cpu.c tables.c memory.c lc.c video.c rect.c ext80col.c mouse.c \
hdisk.c tm2ho.c floppy.c switches.c slots.c ramworks.c charset.c arithmetic.c

OBJS = apple.o cpu.o tables.o memory.o lc.o video.o rect.o ext80col.o mouse.o \
hdisk.o tm2ho.o floppy.o switches.o slots.o ramworks.o charset.o arithmetic.o

default: openapple

openapple: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o openapple $(LIBS)

clean:
	rm -f *.o
	rm -f openapple

nobuild: $(OBJS)

