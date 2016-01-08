include Makefile.mk

CFLAGS += -I./simh_7ad57d7
LDFLAGS += -ldl

C_SRCS = dn6600.c udplib.c coupler.c dn6600_caf.c utils.c
H_SRCS = coupler.h  dn6600.h  udplib.h ipc.h dn6600_caf.h utils.h

OBJS  := $(patsubst %.c,%.o,$(C_SRCS))

all : simh dn6600 test

dn6600 : tags $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o dn6600 simh_7ad57d7/simh.a

test : test.o udplib.o
	$(LD) $(LDFLAGS) test.o udplib.o -o test simh_7ad57d7/simh.a

tags : $(C_SRCS) $(H_SRCS)
	-ctags $(C_SRCS) $(H_SRCS) simh_7ad57d7/*.[ch]


clean:
	-rm dn6600 $(OBJS) tags $(C_SRCS:.c=.d) $(wildcard $(C_SRCS:.c=.d.[0-9]*)) test.o test

.PSUEDO: simh

simh :
	(cd ./simh_7ad57d7; $(MAKE))

%.d: %.c
	@set -e; rm -f $@; \
	$(CC) -MM $(CFLAGS) $< > $@.$$$$; \
	grep -v dps8.sha1.txt $@.$$$$ | sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' > $@; \
	rm -f $@.$$$$

-include $(C_SRCS:.c=.d)

