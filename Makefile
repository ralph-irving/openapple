#
#
#

CC = gcc3
#CC = gcc
#CC = icc

#profiling flag
INCS = -I/usr/include/gtk-1.2 -I/usr/include/glib-1.2 -I/usr/lib/glib/include

CFLAGS = -Wall -O3 -ggdb -march=i686 $(INCS) -fomit-frame-pointer
#CFLAGS = -w2 -O3 -xi -ip -tpp6 `gtk-config --cflags`

LIBS = `gtk-config --libs` -lXxf86dga
#LIBS = `gtk-config --libs` `sdl-config --libs`

SRCS = apple.c cpu.c tables.c memory.c lc.c video.c rect.c ext80col.c mouse.c \
hdisk.c tm2ho.c floppy.c switches.c slots.c ramworks.c charset.c arithmetic.c \
xdga.c

OBJS = apple.o cpu.o tables.o memory.o lc.o video.o rect.o ext80col.o mouse.o \
hdisk.o tm2ho.o floppy.o switches.o slots.o ramworks.o charset.o arithmetic.o \
xdga.o

default: openapple

openapple: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o openapple $(LIBS)

clean:
	rm -f *.o
	rm -f openapple

nobuild: $(OBJS)

