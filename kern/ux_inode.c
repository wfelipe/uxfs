/*--------------------------------------------------------------*/
/*--------------------------- ux_inode.c -----------------------*/
/*--------------------------------------------------------------*/

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/locks.h>
#include <linux/smp_lock.h>
#include <asm/uaccess.h>
#include "ux_fs.h"

MODULE_AUTHOR("Steve Pate <spate@veritas.com>");
MODULE_DESCRIPTION("A primitive filesystem for Linux");
MODULE_LICENSE("GPL");

/*
 * This function looks for "name" in the directory "dip". 
 * If found the inode number is returned.
 */

int
ux_find_entry(struct inode *dip, char *name)
{
        struct ux_inode    *uip = (struct ux_inode *)
                                   &dip->i_private;
        struct super_block *sb = dip->i_sb;
        struct buffer_head *bh;
        struct ux_dirent   *dirent;
        int                i, blk = 0;

        for (blk=0 ; blk < uip->i_blocks ; blk++) {
                bh = sb_bread(sb, uip->i_addr[blk]);
                dirent = (struct ux_dirent *)bh->b_data;
                for (i=0 ; i < UX_DIRS_PER_BLOCK ; i++) {
                        if (strcmp(dirent->d_name, name) == 0) {
                                brelse(bh);
                                return dirent->d_ino;
                        }
                        dirent++;
                }
        }
        brelse(bh);
        return 0;
}

/*
 * This function is called in response to an iget(). For 
 * example, we call iget() from ux_lookup().
 */

void
ux_read_inode(struct inode *inode)
{
        struct buffer_head        *bh;
        struct ux_inode           *di;
        unsigned long             ino = inode->i_ino;
        int                       block;

        if (ino < UX_ROOT_INO || ino > UX_MAXFILES) {
                printk("uxfs: Bad inode number %lu\n", ino);
                return;
        }

        /*
         * Note that for simplicity, there is only one 
         * inode per block!
         */

        block = UX_INODE_BLOCK + ino;
        bh = sb_bread(inode->i_sb, block);
        if (!bh) {
                printk("Unable to read inode %lu\n", ino);
                return;
        }

        di = (struct ux_inode *)(bh->b_data);
        inode->i_mode = di->i_mode;
        if (di->i_mode & S_IFDIR) {
                inode->i_mode |= S_IFDIR;
                inode->i_op = &ux_dir_inops;
                inode->i_fop = &ux_dir_operations;
        } else if (di->i_mode & S_IFREG) {
                inode->i_mode |= S_IFREG;
                inode->i_op = &ux_file_inops;
                inode->i_fop = &ux_file_operations;
                inode->i_mapping->a_ops = &ux_aops;
        }
        inode->i_uid = di->i_uid;
        inode->i_gid = di->i_gid;
        inode->i_nlink = di->i_nlink;
        inode->i_size = di->i_size;
        inode->i_blocks = di->i_blocks;
        inode->i_blksize = UX_BSIZE;
        inode->i_atime = di->i_atime;
        inode->i_mtime = di->i_mtime;
        inode->i_ctime = di->i_ctime;
        memcpy(&inode->i_private, di, sizeof(struct ux_inode));
        brelse(bh);
}

/*
 * This function is called to write a dirty inode to disk.
 */

void
ux_write_inode(struct inode *inode, int unused)
{
        unsigned long       ino = inode->i_ino;
        struct ux_inode     *uip = (struct ux_inode *)
                                    &inode->i_private;
        struct buffer_head  *bh;
        __u32               blk;

        if (ino < UX_ROOT_INO || ino > UX_MAXFILES) {
                printk("uxfs: Bad inode number %lu\n", ino);
                return;
        }
        blk = UX_INODE_BLOCK + ino;
        bh = sb_bread(inode->i_sb, blk);
        uip->i_mode = inode->i_mode;
        uip->i_nlink = inode->i_nlink;
        uip->i_atime = inode->i_atime;
        uip->i_mtime = inode->i_mtime;
        uip->i_ctime = inode->i_ctime;
        uip->i_uid = inode->i_uid;
        uip->i_gid = inode->i_gid;
        uip->i_size = inode->i_size;
        memcpy(bh->b_data, uip, sizeof(struct ux_inode));
        mark_buffer_dirty(bh);
        brelse(bh);
}

/*
 * This function gets called when the link count goes to zero.
 */

void
ux_delete_inode(struct inode *inode)
{
        unsigned long         inum = inode->i_ino;
        struct ux_inode       *uip = (struct ux_inode *)
                                      &inode->i_private;
        struct super_block    *sb = inode->i_sb;
        struct ux_fs          *fs = (struct ux_fs *)
                                     sb->s_private;
        struct ux_superblock  *usb = fs->u_sb;
        int                   i;

        usb->s_nbfree += uip->i_blocks;
        for (i=0 ; i < uip->i_blocks ; i++) {
                usb->s_block[uip->i_addr[i]] = UX_BLOCK_FREE;
                uip->i_addr[i] = UX_BLOCK_FREE;
        }
        usb->s_inode[inum] = UX_INODE_FREE;
        usb->s_nifree++;
        sb->s_dirt = 1;
        clear_inode(inode);
}

/*
 * This function is called when the filesystem is being 
 * unmounted. We free the ux_fs structure allocated during 
 * ux_read_super() and free the superblock buffer_head.
 */

void
ux_put_super(struct super_block *s)
{
        struct ux_fs        *fs = (struct ux_fs *)s->s_private;
        struct buffer_head   *bh = fs->u_sbh;

        /*
         * Free the ux_fs structure allocated by ux_read_super
         */

        kfree(fs);
        brelse(bh);
}

/*
 * This function will be called by the df command.
 */

int
ux_statfs(struct super_block *sb, struct statfs *buf)
{
        struct ux_fs         *fs =  (struct ux_fs *)sb->s_private;
        struct ux_superblock *usb = fs->u_sb;

        buf->f_type = UX_MAGIC;
        buf->f_bsize = UX_BSIZE;
        buf->f_blocks = UX_MAXBLOCKS;
        buf->f_bfree = usb->s_nbfree;
        buf->f_bavail = usb->s_nbfree;
        buf->f_files = UX_MAXFILES;
        buf->f_ffree = usb->s_nifree;
        buf->f_fsid.val[0] = kdev_t_to_nr(sb->s_dev);
        buf->f_namelen = UX_NAMELEN;
        return 0;
}

/*
 * This function is called to write the superblock to disk. We
 * simply mark it dirty and then set the s_dirt field of the
 * in-core superblock to 0 to prevent further unnecessary calls.
 */

void
ux_write_super(struct super_block *sb)
{
        struct ux_fs       *fs = (struct ux_fs *)
                                  sb->s_private;
        struct buffer_head *bh = fs->u_sbh;

        if (!(sb->s_flags & MS_RDONLY)) {
                mark_buffer_dirty(bh);
        }
        sb->s_dirt = 0;
}

struct super_operations uxfs_sops = {
        read_inode:        ux_read_inode,
        write_inode:        ux_write_inode,
        delete_inode:        ux_delete_inode,
        put_super:        ux_put_super,
        write_super:        ux_write_super,
        statfs:                ux_statfs,
};

struct super_block *
ux_read_super(struct super_block *s, void *data, int silent)
{
        struct ux_superblock      *usb;
        struct ux_fs              *fs;
        struct buffer_head        *bh;
        struct inode              *inode;
        kdev_t                    dev;

        dev = s->s_dev;
        set_blocksize(dev, UX_BSIZE);
        s->s_blocksize = UX_BSIZE;
        s->s_blocksize_bits = UX_BSIZE_BITS;

        bh = sb_bread(s, 0);
        if(!bh) {
                goto out;
        }
        usb = (struct ux_superblock *)bh->b_data;
        if (usb->s_magic != UX_MAGIC) {
                if (!silent)
                        printk("Unable to find uxfs filesystem\n");
                goto out;
        }
        if (usb->s_mod == UX_FSDIRTY) {
                printk("Filesystem is not clean. Write and "
                       "run fsck!\n");
                goto out;
        }

        /*
         *  We should really mark the superblock to
         *  be dirty and write it back to disk.
         */

        fs = (struct ux_fs *)kmalloc(sizeof(struct ux_fs),
                                     GFP_KERNEL);
        fs->u_sb = usb;
        fs->u_sbh = bh;
        s->s_private = fs;

        s->s_magic = UX_MAGIC;
        s->s_op = &uxfs_sops;

        inode = iget(s, UX_ROOT_INO);
        if (!inode) {
                goto out;
        }
        s->s_root = d_alloc_root(inode);
        if (!s->s_root) {
                iput(inode);
                goto out;
        }

        if (!(s->s_flags & MS_RDONLY)) {
                mark_buffer_dirty(bh);
                s->s_dirt = 1;
        } 
        return s;

out:
        return NULL;
}

static DECLARE_FSTYPE_DEV(uxfs_fs_type, "uxfs", ux_read_super);

static int __init init_uxfs_fs(void)
{
        return register_filesystem(&uxfs_fs_type);
}

static void __exit exit_uxfs_fs(void)
{
        unregister_filesystem(&uxfs_fs_type);
}

module_init(init_uxfs_fs)
module_exit(exit_uxfs_fs)

