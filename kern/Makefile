obj-$(CONFIG_UXFS) = uxfs.o

uxfs-y = ux_dir.o ux_alloc.o ux_file.o ux_inode.o

KDIR = /lib/modules/$(shell uname -r)/build
PWD = $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) CONFIG_UXFS=m modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) CONFIG_UXFS=m clean
