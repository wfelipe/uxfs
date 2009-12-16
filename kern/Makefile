KERNELDIR = /usr/src/linux-2.4.18-14

include $(KERNELDIR)/.config

FLAGS = -D__KERNEL__ -DEXPORT_SYMTAB -DMODULE $(VERCFLAGS)
GLOBAL_CFLAGS = -g -I$(KERNELDIR)/include $(FLAGS)

M_OBJS = ux_dir.o ux_alloc.o ux_file.o ux_inode.o

M_TARGET = uxfs

SRCS = $(M_OBJS:.o=.c)

CFLAGS = $(GLOBAL_CFLAGS) $(EXTRA_CFLAGS)

$(M_TARGET) : $(M_OBJS)
	ld -r -o $@ $(M_OBJS)

$(M_OBJS) : %.o : %.c
	$(CC) -c $(CFLAGS) -o $@ $<

all: uxfs

clean:
	rm -f $(M_OBJS) $(M_TARGET)
