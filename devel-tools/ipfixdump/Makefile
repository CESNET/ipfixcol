################################################
#    Makefile for the ipfixdump utility        #
################################################

CC      = g++
CPPFLAGS  = -Wall -Wextra -g -DDEBUG
LIBS    = -lfastbit
SRCS    = ipfixdump.cpp \
		Configuration.cpp \
		Column.cpp \
		protocols.cpp \
		Table.cpp \
		Printer.cpp \
		Filter.cpp \
		TableManager.cpp \
		Cursor.cpp
OBJDIR  = .obj
SRCDIR  = src
SRC3RD  = 3rdparty

DOXYGEN = doxygen
DOXYGEN_CONFIG = doxyconf
DOXYGEN_DIR = doc

# scanner must be build before filter (filter includes scanner's header)
OBJS    = $(OBJDIR)/scanner.o $(SRCS:%.cpp=$(OBJDIR)/%.o) $(OBJDIR)/pugi.o

all: ipfixdump

# ipfixdump
ipfixdump: $(OBJS)
	$(CC) $(CPPFLAGS) $(LIBS) $(OBJS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CC) $(CPPFLAGS) -c $< -o $@

$(SRCDIR)/protocols.cpp: /etc/protocols
	cd tools && ./generate_protocols.sh

$(OBJDIR)/pugi.o: $(SRC3RD)/pugixml.cpp
	$(CC) $(CPPFLAGS) -c $< -o $@

# rules to build the lex scanner
$(OBJDIR)/scanner.o: $(SRCDIR)/scanner.c
	@[ -d $$(dirname $@) ] || \
		(mkdir -p $$(dirname $@))
	$(CC) $(CPPFLAGS) -c $< -o $@

$(SRCDIR)/scanner.c: $(SRCDIR)/scanner.l
	flex -o $(SRCDIR)/scanner.c --header-file=$(SRCDIR)/scanner.h $<

.PHONY: doc
doc: doxygen

doxygen: $(DOXYGEN_CONFIG) $(SRCS:%=$(SRCDIR)/%)
	@[ -d $(DOXYGEN_DIR) ] || mkdir -p $(DOXYGEN_DIR); \
	if [ -n "$(DOXYGEN)" ]; then \
		(cat $(DOXYGEN_CONFIG); \
		echo 'OUTPUT_DIRECTORY = $(DOXYGEN_DIR)' ) | \
		$(DOXYGEN) - > /dev/null; \
	else \
		echo "Missing doxygen"; \
		exit 1; \
	fi


clean:
	rm -f ipfixdump $(OBJDIR)/*
	rm -f src/protocols.cpp src/scanner.h src/scanner.c
	rm -fr doc 
