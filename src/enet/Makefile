CFLAGS=-O3 -fomit-frame-pointer
override CFLAGS:= $(CFLAGS) -Iinclude $(shell ./check_cflags.sh $(CC) $(CFLAGS))

OBJS= \
	callbacks.o \
	compress.o \
	host.o \
	list.o \
	packet.o \
	peer.o \
	protocol.o \
	unix.o \
	win32.o

libenet.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

default: libenet.a

clean:
	-$(RM) libenet.a $(OBJS)
	 
