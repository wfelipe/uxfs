/*--------------------------------------------------------------*/
/*---------------------------- uxfs_dir.c ------------------------*/
/*--------------------------------------------------------------*/

#include <linux/sched.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "uxfs.h"

/*
 * Add "name" to the directory "dip"
 */

int uxfs_diradd(struct inode *dip, const char *name, int inum)
{
	struct uxfs_inode *uip = (struct uxfs_inode *)
	    dip->i_private;
	struct buffer_head *bh;
	struct super_block *sb = dip->i_sb;
	struct uxfs_dirent *dirent;
	__u32 blk = 0;
	int i, pos;

	for (blk = 0; blk < uip->i_blocks; blk++) {
		bh = sb_bread(sb, uip->i_addr[blk]);
		dirent = (struct uxfs_dirent *) bh->b_data;
		for (i = 0; i < UXFS_DIRS_PER_BLOCK; i++) {
			if (dirent->d_ino != 0) {
				dirent++;
				continue;
			} else {
				dirent->d_ino = inum;
				strcpy(dirent->d_name, name);
				mark_buffer_dirty(bh);
				mark_inode_dirty(dip);	//this shouldn't be necessary...
				brelse(bh);
				return 0;
			}
		}
		brelse(bh);
	}

	/*
	 * We didn't find an empty slot so need to allocate 
	 * a new block if there's space in the inode.
	 */

	if (uip->i_blocks < UXFS_DIRECT_BLOCKS) {
		pos = uip->i_blocks;
		blk = uxfs_block_alloc(sb);
		uip->i_blocks++;
		uip->i_size += UXFS_BSIZE;
		dip->i_size += UXFS_BSIZE;
		dip->i_blocks++;
		uip->i_addr[pos] = blk;
		bh = sb_bread(sb, blk);
		memset(bh->b_data, 0, UXFS_BSIZE);
		mark_inode_dirty(dip);
		dirent = (struct uxfs_dirent *) bh->b_data;
		dirent->d_ino = inum;
		strcpy(dirent->d_name, name);
		mark_buffer_dirty(bh);
		brelse(bh);
	}

	return 0;
}

/*
 * Remove "name" from the specified directory.
 */

int uxfs_dirdel(struct inode *dip, char *name)
{
	struct uxfs_inode *uip = (struct uxfs_inode *)
	    dip->i_private;
	struct buffer_head *bh;
	struct super_block *sb = dip->i_sb;
	struct uxfs_dirent *dirent;
	__u32 blk = 0;
	int i;

	while (blk < uip->i_blocks) {
		bh = sb_bread(sb, uip->i_addr[blk]);
		blk++;
		dirent = (struct uxfs_dirent *) bh->b_data;
		for (i = 0; i < UXFS_DIRS_PER_BLOCK; i++) {
			if (strcmp(dirent->d_name, name) != 0) {
				dirent++;
				continue;
			} else {
				dirent->d_ino = 0;
				dirent->d_name[0] = '\0';
				mark_buffer_dirty(bh);	//unnecessary??
				inode_dec_link_count(dip);
				//      mark_inode_dirty(dip); redundant
				break;
			}
		}
		brelse(bh);
	}
	return 0;
}

int uxfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	unsigned long pos;
	struct inode *inode = filp->f_dentry->d_inode;
	struct uxfs_inode *uip = (struct uxfs_inode *)
	    inode->i_private;
	struct uxfs_dirent *udir;
	struct buffer_head *bh;
	__u32 blk;

      start_again:
	pos = filp->f_pos;
	if (pos >= inode->i_size)
		return 0;
	blk = (pos + 1) / UXFS_BSIZE;
	blk = uip->i_addr[blk];
	bh = sb_bread(inode->i_sb, blk);
	udir = (struct uxfs_dirent *) (bh->b_data + pos % UXFS_BSIZE);

	/*
	 * Skip over 'null' directory entries.
	 */

	if (udir->d_ino == 0) {
		filp->f_pos += sizeof(struct uxfs_dirent);
		brelse(bh);
		goto start_again;
	} else {
		filldir(dirent, udir->d_name,
			sizeof(udir->d_name), pos, udir->d_ino,
			DT_UNKNOWN);
	}
	filp->f_pos += sizeof(struct uxfs_dirent);
	brelse(bh);
	return 0;
}

struct file_operations uxfs_dir_operations = {
	.read = generic_read_dir,
	.readdir = uxfs_readdir,
	.fsync = noop_fsync,
};

/*
 * When we reach this point, uxfs_lookup () has already been called
 * to create a negative entry in the dcache. Thus, we need to
 * allocate a new inode on disk and associate it with the dentry.
 */

int uxfs_create(struct inode *dip, struct dentry *dentry, umode_t mode,
		struct nameidata *nd)
{
	struct uxfs_inode *nip;
	struct super_block *sb = dip->i_sb;
	struct inode *inode;
	ino_t inum = 0;

	/*
	 * See if the entry exists. If not, create a new 
	 * disk inode, and incore inode. The add the new 
	 * entry to the directory.
	 */

	inum = uxfs_find_entry(dip, (char *) dentry->d_name.name);
	if (inum)
		return -EEXIST;
	inode = new_inode(sb);
	if (!inode)
		return -ENOSPC;
	inum = uxfs_ialloc(sb);
	if (!inum) {
		iput(inode);
		return -ENOSPC;
	}
	uxfs_diradd(dip, (char *) dentry->d_name.name, inum);

	/*
	 * Increment the parent link count and intialize the inode.
	 */

	//inode_inc_link_count(inode); //this method breaks the fs. setting n_link later works correctly
	inode->i_uid = current_fsuid();
	inode->i_gid =
	    (dip->i_mode & S_ISGID) ? dip->i_gid : current_fsgid();
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	inode->i_op = &uxfs_file_inops;
	inode->i_fop = &uxfs_file_operations;
	inode->i_mapping->a_ops = &uxfs_aops;
	inode->i_mode = mode;
	set_nlink(inode, 1);
	inode->i_ino = inum;

	//need to set i_private
	inode->i_private = uxfs_i(inode);

	nip = (struct uxfs_inode *) inode->i_private;
	nip->i_mode = mode;
	nip->i_nlink = 1;
	nip->i_atime = nip->i_ctime = nip->i_mtime = CURRENT_TIME.tv_sec;
	nip->i_uid = inode->i_uid;
	nip->i_gid = inode->i_gid;
	nip->i_size = 0;
	nip->i_blocks = 0;
	memset(nip->i_addr, 0,
	       UXFS_DIRECT_BLOCKS * sizeof(nip->i_addr[0]));

	insert_inode_hash(inode);	//moved from above
	d_instantiate(dentry, inode);
	//  mark_inode_dirty(dip); //this does not belong here
	mark_inode_dirty(inode);
	return 0;
}

/*
 * Make a new directory. We already have a negative dentry
 * so must create the directory and instantiate it.
 */

int uxfs_mkdir(struct inode *dip, struct dentry *dentry, umode_t mode)
{
	struct uxfs_inode *nip;
	struct buffer_head *bh;
	struct super_block *sb = dip->i_sb;
	struct uxfs_dirent *dirent;
	struct inode *inode;
	ino_t inum = 0;
	int blk;

	/*
	 * Make sure there isn't already an entry. If not, 
	 * allocate one, a new inode and new incore inode.
	 */

	inum = uxfs_find_entry(dip, (char *) dentry->d_name.name);
	if (inum)
		return -EEXIST;
	inode = new_inode(sb);
	if (!inode)
		return -ENOSPC;
	inum = uxfs_ialloc(sb);
	if (!inum) {
		iput(inode);
		return -ENOSPC;
	}
	uxfs_diradd(dip, (char *) dentry->d_name.name, inum);

	inode->i_uid = current_fsuid();
	inode->i_gid =
	    (dip->i_mode & S_ISGID) ? dip->i_gid : current_fsgid();
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	inode->i_blocks = 1;
//      inode->i_blksize = UXFS_BSIZE;
	inode->i_op = &uxfs_dir_inops;
	inode->i_fop = &uxfs_dir_operations;
	inode->i_mapping->a_ops = &uxfs_aops;
	inode->i_mode = mode | S_IFDIR;
	inode->i_ino = inum;
	inode->i_size = UXFS_BSIZE;
	inode->i_private = uxfs_i(inode);	//initialize private, again!
	set_nlink(inode, 2);

	nip = (struct uxfs_inode *) inode->i_private;
	nip->i_mode = mode | S_IFDIR;
	nip->i_nlink = 2;
	nip->i_atime = nip->i_ctime = nip->i_mtime = CURRENT_TIME.tv_sec;
	nip->i_uid = current_fsuid();
	nip->i_gid =
	    (dip->i_mode & S_ISGID) ? dip->i_gid : current_fsgid();
	nip->i_size = 512;
	nip->i_blocks = 1;
	memset(nip->i_addr, 0,
	       UXFS_DIRECT_BLOCKS * sizeof(nip->i_addr[0]));

	blk = uxfs_block_alloc(sb);
	nip->i_addr[0] = blk;
	bh = sb_bread(sb, blk);
	memset(bh->b_data, 0, UXFS_BSIZE);
	dirent = (struct uxfs_dirent *) bh->b_data;
	dirent->d_ino = inum;
	strcpy(dirent->d_name, ".");
	dirent++;
	dirent->d_ino = inode->i_ino;
	strcpy(dirent->d_name, "..");

	mark_buffer_dirty(bh);
	brelse(bh);
	insert_inode_hash(inode);
	d_instantiate(dentry, inode);
	mark_inode_dirty(inode);

	/*
	 * Increment the link count of the parent directory.
	 */

	inode_inc_link_count(dip);
	mark_inode_dirty(dip);
	return 0;
}

/*
 * Remove the specified directory.
 */

int uxfs_rmdir(struct inode *dip, struct dentry *dentry)
{
	struct super_block *sb = dip->i_sb;
	struct uxfs_fs *fs = (struct uxfs_fs *) sb->s_fs_info;
	struct uxfs_superblock *usb = fs->u_sb;
	struct inode *inode = dentry->d_inode;
	struct uxfs_inode *uip = (struct uxfs_inode *)
	    inode->i_private;
	int inum, i;

	if (inode->i_nlink > 2)
		return -ENOTEMPTY;

	/*
	 * Remove the entry from the parent directory
	 */

	inum = uxfs_find_entry(dip, (char *) dentry->d_name.name);
	if (!inum)
		return -ENOTDIR;
	uxfs_dirdel(dip, (char *) dentry->d_name.name);

	/*
	 * Clean up the inode
	 */

	for (i = 0; i < UXFS_DIRECT_BLOCKS; i++) {
		if (uip->i_addr[i] != 0) {
			usb->s_block[uip->i_addr[i]]
			    = UXFS_BLOCK_FREE;
			usb->s_nbfree++;
		}
	}

	/*
	 * Update the superblock summaries.
	 */

	usb->s_inode[dip->i_ino] = UXFS_INODE_FREE;
	usb->s_nifree++;
	return 0;
}

/*
 * Lookup the specified file. A call is made to uxfs_iget () to
 * bring the inode into core.
 */

struct dentry *uxfs_lookup(struct inode *dip, struct dentry *dentry,
			   struct nameidata *nd)
{
	struct inode *inode = NULL;
	int inum;

	if (dentry->d_name.len > UXFS_NAMELEN)
		return ERR_PTR(-ENAMETOOLONG);

	inum = uxfs_find_entry(dip, (char *) dentry->d_name.name);
	if (inum) {
		inode = uxfs_iget(dip->i_sb, inum);
		if (!inode)
			return ERR_CAST(inode);
	}
	d_add(dentry, inode);
	return NULL;
}

/*
 * Called in response to an ln command/syscall.
 */

int uxfs_link(struct dentry *old, struct inode *dip, struct dentry *new)
{
	struct inode *inode = old->d_inode;
	int error;

	/*
	 * Add the new file (new) to its parent directory (dip)
	 */

	error = uxfs_diradd(dip, new->d_name.name, inode->i_ino);

	/*
	 * Increment the link count of the target inode
	 */

	inode_inc_link_count(inode);
	mark_inode_dirty(inode);
	atomic_inc(&inode->i_count);
	d_instantiate(new, inode);
	return 0;
}

/*
 * Called to remove a file (decrement its link count)
 */

int uxfs_unlink(struct inode *dip, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;

	uxfs_dirdel(dip, (char *) dentry->d_name.name);
	inode_dec_link_count(inode);
	mark_inode_dirty(inode);	//more redundancy,
	return 0;
}

struct inode_operations uxfs_dir_inops = {
	.create = uxfs_create,
	.lookup = uxfs_lookup,
	.mkdir = uxfs_mkdir,
	.rmdir = uxfs_rmdir,
	.link = uxfs_link,
	.unlink = uxfs_unlink,
};
