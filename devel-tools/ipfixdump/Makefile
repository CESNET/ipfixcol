################################################
#    Makefile for the ipfixdump utility        #
################################################

CC      = g++
CPPFLAGS  = -Wall -Wextra -g -DDEBUG
LIBS    = -lfastbit
SRCS    = ipfixdump.cpp \
		configuration.cpp \
		data.cpp \
		printer.cpp \
		columnFormat.cpp \
		protocols.cpp \
		filter.cpp
OBJDIR  = .obj
SRCDIR  = src
SRC3RD  = 3rdparty

# scanner must be build before filter (filter includes scanner's header)
OBJS    = $(OBJDIR)/scanner.o $(SRCS:%.cpp=$(OBJDIR)/%.o) $(OBJDIR)/pugi.o

all: ipfixdump

# ipfixdump
ipfixdump: $(OBJS)
	$(CC) $(CPPFLAGS) $(LIBS) $(OBJS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@[ -d $$(dirname $@) ] || \
		(mkdir -p $$(dirname $@))
	$(CC) $(CPPFLAGS) -c $< -o $@

$(SRCDIR)/protocols.cpp: /etc/protocols
	cd tools && ./generate_protocols.sh

$(OBJDIR)/pugi.o: $(SRC3RD)/pugixml.cpp
	$(CC) $(CPPFLAGS) -c $< -o $@

# rules to build the lex scanner
$(OBJDIR)/scanner.o: $(SRCDIR)/scanner.c
	$(CC) $(CPPFLAGS) -c $< -o $@

$(SRCDIR)/scanner.c: $(SRCDIR)/scanner.l
	flex -o $(SRCDIR)/scanner.c --header-file=$(SRCDIR)/scanner.h $<

clean:
	rm -f ipfixdump $(OBJDIR)/*
	rm -f src/protocols.cpp src/scanner.h src/scanner.c
