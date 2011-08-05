################################################
# Makefile for the ipfixcol input plugin test  #
################################################

CC      = g++
CPPFLAGS  = -Wall -I/usr/include/libxml2 -Wextra -g -DDEBUG
LIBS    = -ldl  -lxml2 -lfastbit
SRCS    = ipfixdump.cpp \
		configuration.cpp \
		data.cpp \
		printer.cpp \
		protocols.cpp
OBJDIR  = .obj
SRCDIR  = src

OBJS    = $(SRCS:%.cpp=$(OBJDIR)/%.o)

all: ipfixdump

# ipfixdump
ipfixdump: $(OBJS)
	$(CC) $(CPPFLAGS) $(LIBS) $(OBJS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@[ -d $$(dirname $@) ] || \
		(mkdir -p $$(dirname $@))
	$(CC) $(CPPFLAGS) -c $< -o $@

$(SRCDIR)/protocols.cpp:
	cd tools && ./generate_protocols.sh

clean:
	rm -f ipfixdump $(OBJDIR)/*
	rm -f src/protocols.cpp
