

CXX	:= g++
CC  := gcc
LD	:= g++
AR	:= ar
CXXFLAGS := -Wall -O2 -Werror -g
INCLUDES = 

TARGET	= a.out
LINKS	= 

SOURCES := main.cpp
SOURCES += http_parser.c


OBJS := $(SOURCES:.cc=.o)

all: prebuild $(TARGET) simtv

$(TARGET): $(OBJS)
	@echo Linking $@ ...
	$(LD) $(OBJS) $(LINKS) $(LIBS) -o$@
	@echo -------------------------------------------
	@echo done.

simtv: simtv.cpp
	@echo Linking $@ ...
	$(CXX) $< $(INCLUDES) $(CXXFLAGS) $(LINKS) $(LIBS) -o$@
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
	rm -fr *.o a.out simtv



