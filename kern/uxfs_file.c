/*--------------------------------------------------------------*/
/*--------------------------- uxfs_file.c ------------------------*/
/*--------------------------------------------------------------*/

#include <linux/fs.h>
#include <linux/buffer_head.h>
#include "uxfs.h"
#include <linux/aio.h>

struct file_operations uxfs_file_operations = {
	.llseek = generic_file_llseek,
	.read = do_sync_read,
	.aio_read = generic_file_aio_read,	//added 
	.write = do_sync_write,
	.aio_write = generic_file_aio_write,	//added
	.mmap = generic_file_mmap,
	.splice_read = generic_file_splice_read,	//added
};

int uxfs_get_block(struct inode *inode,
		   sector_t iblock, struct buffer_head *bh_result,
		   int create)
{
	struct super_block *sb = inode->i_sb;
	struct uxfs_inode *uip = (struct uxfs_inode *) inode->i_private;
	__u32 blk;

	/*
	 * First check to see is the file can be extended.
	 */

	if (iblock >= UXFS_DIRECT_BLOCKS)
		return -EFBIG;

	/*
	 * If we're creating, we must allocate a new block.
	 */

	if (create) {
		blk = uxfs_block_alloc(sb);
		if (blk == 0) {
			printk(KERN_ERR "uxfs: uxfs_get_block - "
			       "Out of space\n");
			return -ENOSPC;
		}
		uip->i_addr[iblock] = blk;
		uip->i_blocks++;
		uip->i_size = inode->i_size;
		mark_inode_dirty(inode);
	}
	bh_result->b_bdev = sb->s_bdev;	//changed from inode->i_bdev (which was null and stupid)
	bh_result->b_blocknr = uip->i_addr[iblock];
	bh_result->b_state |= (1UL << BH_Mapped);

	return 0;
}

int uxfs_writepage(struct page *page, struct writeback_control *wbc)
{
	return block_write_full_page(page, uxfs_get_block, wbc);
}

int uxfs_readpage(struct file *file, struct page *page)
{
	return block_read_full_page(page, uxfs_get_block);
}

int uxfs_write_begin(struct file *file, struct address_space *mapping,
		     loff_t pos, unsigned len, unsigned flags,
		     struct page **pagep, void **fsdata)
{
	return block_write_begin(file->f_mapping, pos, len, flags, pagep,
				 uxfs_get_block);
}

sector_t uxfs_bmap(struct address_space * mapping, sector_t block)
{
	return generic_block_bmap(mapping, block, uxfs_get_block);
}

struct address_space_operations uxfs_aops = {
	.readpage = uxfs_readpage,
	.writepage = uxfs_writepage,
	.write_begin = uxfs_write_begin,
	.write_end = generic_write_end,
	.bmap = uxfs_bmap,
};

struct inode_operations uxfs_file_inops = {
	.link = uxfs_link,
	.unlink = uxfs_unlink,
};
