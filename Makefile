include Makefile.mk

CFLAGS += -I./simh_7ad57d7
LDFLAGS += -ldl

all : simh dn6600

dn6600 : dn6600.o 
	$(LD) $(LDFLAGS) dn6600.o -o dn6600 simh_7ad57d7/simh.a

dn6600.o : dn6600.c simh
	$(CC) $(CFLAGS) -c dn6600.c

.PSUEDO: simh

simh :
	(cd ./simh_7ad57d7; $(MAKE))
