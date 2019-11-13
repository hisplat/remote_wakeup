

CXX	:= g++
CC  := gcc
LD	:= g++
AR	:= ar
CXXFLAGS := -Wall -O2 -Werror
INCLUDES = -Ilog -I.

TARGET	= a.out
LINKS	= 

SOURCES := main.cpp
SOURCES += http_parser.c


OBJS := $(SOURCES:.cc=.o)

all: prebuild $(TARGET) $(READLOG)

$(TARGET): $(OBJS)
	@echo Linking $@ ...
	$(LD) $(OBJS) $(LINKS) $(LIBS) -o$@
	@echo -------------------------------------------
	@echo done.

.cpp.o:
	@echo Compling $@ ...
	$(CXX) -c $< $(INCLUDES) $(CXXFLAGS)  -o $@
	@echo -------------------------------------------

.c.o:
	@echo Compling $@ ...
	$(CC) -c $< $(INCLUDES) $(CFLAGS)  -o $@
	@echo ------------------------------------------


.PHONY: prebuild clean
prebuild:

clean:
	rm -fr $(OBJS) $(DEPS) readlog.o wincurse.o



