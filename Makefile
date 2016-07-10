PREFIX		= /usr
BINDIR		= ${PREFIX}/bin

CC		= gcc
INSTALL		= install

CFLAGS		= -Wall -Werror
CPPFLAGS	= -MMD
LDFLAGS		=

PROGNAME	= httpget
SRC_FILES	= $(wildcard *.c)
OBJ_FILES	= $(SRC_FILES:.c=.o)
DEP_FILES	= $(SRC_FILES:.c=.d)

PHONY += all
all: $(PROGNAME)

$(PROGNAME): $(OBJ_FILES)
	$(CC) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

-include $(DEP_FILES)

PHONY += clean
clean:
	$(RM) $(OBJ_FILES) $(DEP_FILES) $(PROGNAME)

PHONY += install
install: $(PROGNAME)
	$(INSTALL) -D -s $(PROGNAME) $(BINDIR)/$(PROGNAME)

.PHONY: $(PHONY)
