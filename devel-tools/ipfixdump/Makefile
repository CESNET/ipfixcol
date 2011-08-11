################################################
# Makefile for the ipfixcol input plugin test  #
################################################

CC      = g++
CPPFLAGS  = -Wall -I/usr/include/libxml2 -Wextra -g -DDEBUG
LIBS    = -lfastbit
SRCS    = ipfixdump.cpp \
		configuration.cpp \
		data.cpp \
		printer.cpp \
		columnFormat.cpp \
		protocols.cpp
OBJDIR  = .obj
SRCDIR  = src
SRC3RD  = 3rdparty

OBJS    = $(SRCS:%.cpp=$(OBJDIR)/%.o) $(OBJDIR)/pugi.o

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

$(OBJDIR)/pugi.o: $(SRC3RD)/pugixml.cpp
	$(CC) $(CPPFLAGS) -c $< -o $@

clean:
	rm -f ipfixdump $(OBJDIR)/*
	rm -f src/protocols.cpp
