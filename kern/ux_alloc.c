/*--------------------------------------------------------------*/
/*--------------------------- ux_alloc.c -----------------------*/
/*--------------------------------------------------------------*/

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/locks.h>
#include <linux/smp_lock.h>
#include <asm/uaccess.h>
#include "ux_fs.h"

/*
 * Allocate a new inode. We update the superblock and return
 * the inode number.
 */

ino_t
ux_ialloc(struct super_block *sb)
{
        struct ux_fs          *fs = (struct ux_fs *)sb->s_private;
        struct ux_superblock  *usb = fs->u_sb;
        int                   i;

        if (usb->s_nifree == 0) {
                printk("uxfs: Out of inodes\n");
                return 0;
        }
        for (i = 3 ; i < UX_MAXFILES ; i++) {
                if (usb->s_inode[i] == UX_INODE_FREE) {
                        usb->s_inode[i] = UX_INODE_INUSE;
                        usb->s_nifree--;
                        sb->s_dirt = 1;
                        return i;
                }
        }
        printk("uxfs: ux_ialloc - We should never reach here\n");
        return 0;
}

/*
 * Allocate a new data block. We update the superblock and return
 * the new block  number.
 */

__u32
ux_block_alloc(struct super_block *sb)
{
        struct ux_fs          *fs = (struct ux_fs *)sb->s_private;
        struct ux_superblock  *usb = fs->u_sb;
        int                   i;

        if (usb->s_nbfree == 0) {
                printk("uxfs: Out of space\n");
                return 0;
        }

        /*
         * Start looking at block 1. Block 0 is 
         * for the root directory.
         */

        for (i = 1 ; i < UX_MAXBLOCKS ; i++) {
                if (usb->s_block[i] == UX_BLOCK_FREE) {
                        usb->s_block[i] = UX_BLOCK_INUSE;
                        usb->s_nbfree--;
                        sb->s_dirt = 1;
                        return UX_FIRST_DATA_BLOCK + i;
                }
        }
        printk("uxfs: ux_block_alloc - "
               "We should never reach here\n");
        return 0;
}

