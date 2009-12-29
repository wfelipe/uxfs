/*--------------------------------------------------------------*/
/*--------------------------- ux_file.c ------------------------*/
/*--------------------------------------------------------------*/

#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/smp_lock.h>
#include "ux_fs.h"

struct file_operations ux_file_operations = {
	.llseek		= generic_file_llseek,
	.read		= do_sync_read,
	.write		= do_sync_write,
	.mmap		= generic_file_mmap,
};

int ux_get_block (struct inode *inode,
				sector_t iblock, struct buffer_head *bh_result,
			   	int create)
{
	struct super_block *sb = inode->i_sb;
	struct ux_inode *uip = (struct ux_inode *) &inode->i_private;
   	__u32 blk;

	/*
	 * First check to see is the file can be extended.
	 */

	if (iblock >= UX_DIRECT_BLOCKS)
		return -EFBIG;

	/*
	 * If we're creating, we must allocate a new block.
	 */

	if (create)
	{
		blk = ux_block_alloc (sb);
		if (blk == 0)
		{
			printk(KERN_ERR "uxfs: ux_get_block - "
				"Out of space\n");
			return -ENOSPC;
		}
		uip->i_addr[iblock] = blk;
		uip->i_blocks++;
		uip->i_size = inode->i_size;
		mark_inode_dirty (inode);
	}
	bh_result->b_bdev = inode->i_bdev;
	bh_result->b_blocknr = uip->i_addr[iblock];
	bh_result->b_state |= (1UL << BH_Mapped);

	return 0;
}

int ux_writepage (struct page *page, struct writeback_control *wbc)
{
	return block_write_full_page (page, ux_get_block, wbc);
}

int ux_readpage (struct file *file, struct page *page)
{
	return block_read_full_page (page, ux_get_block);
}

int ux_write_begin (struct file *file, struct address_space *mapping,
		loff_t pos, unsigned len, unsigned flags,
		struct page **pagep, void **fsdata)
{
	return block_write_begin (file, mapping, pos, len, flags, pagep, fsdata, ux_get_block);
}

sector_t ux_bmap (struct address_space *mapping, sector_t block)
{
	return generic_block_bmap (mapping, block, ux_get_block);
}

struct address_space_operations ux_aops = {
	.readpage		= ux_readpage,
	.writepage		= ux_writepage,
	.sync_page		= block_sync_page,
	.write_begin		= ux_write_begin,
	.write_end		= generic_write_end,
	.bmap			= ux_bmap,
};

struct inode_operations ux_file_inops = {
	.link		= ux_link,
	.unlink		= ux_unlink,
};

