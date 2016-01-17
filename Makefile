# A simple makefile to make or test the apple example. Just uses the Make that's standard on Mac.
# part of libsvgtiny. M.I.T. license

SVGTINY_C= libsvgtiny/src/svgColor2.c \
  libsvgtiny/src/svgtiny.c \
  libsvgtiny/src/svgtiny_gradient.c \
  libsvgtiny/src/svgtiny_list.c \
  libsvgtiny/src/xml2dom.c

SVGTINY_O= libsvgtiny/src/svgColor2.o \
  libsvgtiny/src/svgtiny.o \
  libsvgtiny/src/svgtiny_gradient.o \
  libsvgtiny/src/svgtiny_list.o \
  libsvgtiny/src/xml2dom.o


SVGTINY_H= libsvgtiny/include/svgtiny.h libsvgtiny/src/svgtiny_internal.h \
  libsvgtiny/src/svgtiny_strings.h \
  libsvgtiny/src/xml2dom.h

SVGTINYWRITER_C= libsvgtinywriter/src/svgtiny_writer.c \
  libsvgtinywriter/src/svgtiny_report_err.c

SVGTINYWRITER_O= libsvgtinywriter/src/svgtiny_writer.o \
  libsvgtinywriter/src/svgtiny_report_err.o

SVGTINYWRITER_H= libsvgtinywriter/include/svgtiny_writer.h \
  libsvgtinywriter/include/svgtiny_report_err.h

SVGDIRECTORIES= -I/usr/include/libxml2 -Ilibsvgtinywriter/include -Ilibsvgtiny/include -I.
CFLAGS= $(SVGDIRECTORIES) -DUSE_XML2
all: bin/apple_main bin/satinstitch

bin/libsvgtiny.a : $(SVGTINY_O) $(SVGTINY_H)
	- mkdir bin
	ar rs bin/libsvgtiny.a $(SVGTINY_O)

bin/libsvgtinywriter.a : $(SVGTINYWRITER_O) $(SVGTINYWRITER_H)
	- mkdir bin
	ar rs bin/libsvgtinywriter.a $(SVGTINYWRITER_O)

bin/apple_main : bin/libsvgtiny.a bin/libsvgtinywriter.a examples/apple_main.c
	cc $(CFLAGS)  -o bin/apple_main examples/apple_main.c bin/libsvgtiny.a bin/libsvgtinywriter.a -lxml2

bin/satinstitch : bin/libsvgtiny.a bin/libsvgtinywriter.a examples/satinstitch.c
	cc $(CFLAGS)  -o bin/satinstitch examples/satinstitch.c bin/libsvgtiny.a bin/libsvgtinywriter.a -lxml2

test: bin/apple_main
	bin/apple_main

clean:
	rm -f $(SVGTINY_O) $(SVGTINYWRITER_O) bin/libsvgtinywriter.a bin/libsvgtiny.a

pristine: clean
	rm -f bin/apple_main bin/satinstitch
	rmdir bin

