OPT=-O2
#CFLAGS=$(OPT) -Wall -Wextra -Wno-parentheses -Wstrict-prototypes -Wmissing-prototypes -ggdb
CFLAGS=$(OPT) -Wall -Wextra -Wno-parentheses -ggdb -D_GNU_SOURCE

ifeq ($(ARCH),arm)
CC?=arm-none-linux-gnueabi-gcc
else
CC?=gcc
endif

all: mrpd

VPATH = ../common
INCFLAGS += -I../common

mrpd: mrpd.o mvrp.o msrp.o mmrp.o mrp.o ../common/parse.o ../common/eui64set.o

%.o: %.c
	$(CC) -c $(INCFLAGS) $(CFLAGS) -o $@  $<
%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	rm -f `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
	rm -f mrpd *.o

indent:
	indent --linux-style mrpd.c mrpd.h mvrp.c mvrp.h msrp.c msrp.h mmrp.c mmrp.h mrp.c mrp.h \
		mrpw.c que.c que.h ../common/parse.c ../common/parse.h ../common/eui64set.c ../common/eui64set.h

