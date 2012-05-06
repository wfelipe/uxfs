obj-$(CONFIG_UXFS_FS) = uxfs.o

uxfs-y := uxfs_dir.o uxfs_alloc.o uxfs_file.o uxfs_inode.o

KDIR = /lib/modules/$(shell uname -r)/build
PWD = $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) CONFIG_UXFS_FS=m modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) CONFIG_UXFS_FS=m clean
