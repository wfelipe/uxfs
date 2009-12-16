/*--------------------------------------------------------------*/
/*--------------------------- ux_file.c ------------------------*/
/*--------------------------------------------------------------*/

#include <linux/fs.h>
#include <linux/locks.h>
#include <linux/smp_lock.h>
#include "ux_fs.h"

struct file_operations ux_file_operations = {
        llseek:    generic_file_llseek,
        read:      generic_file_read,
        write:     generic_file_write,
        mmap:      generic_file_mmap,
};

int
ux_get_block(struct inode *inode, long block, 
             struct buffer_head *bh_result, int create)
{
        struct super_block *sb = inode->i_sb;
        struct ux_fs       *fs = (struct ux_fs *)
                                  sb->s_private;
        struct ux_inode    *uip = (struct ux_inode *)
                                   &inode->i_private;
        __u32              blk;

        /*
         * First check to see is the file can be extended.
         */

        if (block >= UX_DIRECT_BLOCKS) {
                return -EFBIG;
        }

        /*
         * If we're creating, we must allocate a new block.
         */

        if (create) {
                blk = ux_block_alloc(sb);
                if (blk == 0) {
                        printk("uxfs: ux_get_block - "
                               "Out of space\n");
                        return -ENOSPC;
                }
                uip->i_addr[block] = blk;
                uip->i_blocks++;
                uip->i_size = inode->i_size;
                mark_inode_dirty(inode);
        }
        bh_result->b_dev = inode->i_dev;
        bh_result->b_blocknr = uip->i_addr[block];
        bh_result->b_state |= (1UL << BH_Mapped);
        return 0;
}

int
ux_writepage(struct page *page)
{
        return block_write_full_page(page, ux_get_block);
}

int
ux_readpage(struct file *file, struct page *page)
{
        return block_read_full_page(page, ux_get_block);
}

int
ux_prepare_write(struct file *file, struct page *page,
                 unsigned from, unsigned to)
{
        return block_prepare_write(page, from, to, ux_get_block);
}

int
ux_bmap(struct address_space *mapping, long block)
{
        return generic_block_bmap(mapping, block, ux_get_block);
}

struct address_space_operations ux_aops = {
        readpage:         ux_readpage,
        writepage:        ux_writepage,
        sync_page:        block_sync_page,
        prepare_write:    ux_prepare_write,
        commit_write:     generic_commit_write,
        bmap:             ux_bmap,
};

struct inode_operations ux_file_inops = {
        link:             ux_link,
        unlink:           ux_unlink,
};

