/*--------------------------------------------------------------*/
/*---------------------------- ux_dir.c ------------------------*/
/*--------------------------------------------------------------*/

#include <linux/sched.h>
#include <linux/string.h>
#include <linux/locks.h>

#include "ux_fs.h"

/*
 * Add "name" to the directory "dip"
 */

int
ux_diradd(struct inode *dip, const char *name, int inum)
{
        struct ux_inode       *uip = (struct ux_inode *)
                                      &dip->i_private;
        struct buffer_head    *bh;
        struct super_block    *sb = dip->i_sb;
        struct ux_dirent      *dirent;
        __u32                 blk = 0;
        int                   i, pos;

        for (blk=0 ; blk < uip->i_blocks ; blk++) {
                bh = sb_bread(sb, uip->i_addr[blk]);
                dirent = (struct ux_dirent *)bh->b_data;
                for (i=0 ; i < UX_DIRS_PER_BLOCK ; i++) {
                        if (dirent->d_ino != 0) {
                                dirent++;
                                continue;
                        } else {
                                dirent->d_ino = inum;
                                strcpy(dirent->d_name, name);
                                mark_buffer_dirty(bh);
                                mark_inode_dirty(dip);
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

        if (uip->i_blocks < UX_DIRECT_BLOCKS) {
                pos = uip->i_blocks;
                blk = ux_block_alloc(sb);
                uip->i_blocks++;
                uip->i_size += UX_BSIZE;
                dip->i_size += UX_BSIZE;
                dip->i_blocks++;
                uip->i_addr[pos] = blk;
                bh = sb_bread(sb, blk);
                memset(bh->b_data, 0, UX_BSIZE);
                mark_inode_dirty(dip);
                dirent = (struct ux_dirent *)bh->b_data;
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

int
ux_dirdel(struct inode *dip, char *name)
{
        struct ux_inode         *uip = (struct ux_inode *)
                                        &dip->i_private;
        struct buffer_head      *bh;
        struct super_block      *sb = dip->i_sb;
        struct ux_dirent        *dirent;
        __u32                   blk = 0;
        int                     i;

        while (blk < uip->i_blocks) {
                bh = sb_bread(sb, uip->i_addr[blk]);
                blk++;
                dirent = (struct ux_dirent *)bh->b_data;
                for (i=0 ; i < UX_DIRS_PER_BLOCK ; i++) {
                        if (strcmp(dirent->d_name, name) != 0) {
                                dirent++;
                                continue;
                        } else {
                                dirent->d_ino = 0;
                                dirent->d_name[0] = '\0';
                                mark_buffer_dirty(bh);
                                dip->i_nlink--;
                                mark_inode_dirty(dip);
                                break;
                        }
                }
                brelse(bh);
        }
        return 0;
}

int
ux_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
        unsigned long         pos;
        struct inode          *inode = filp->f_dentry->d_inode;
        struct ux_inode       *uip = (struct ux_inode *)
                                      &inode->i_private;
        struct ux_dirent      *udir;
        struct buffer_head    *bh;
        __u32                 blk;

start_again:
        pos = filp->f_pos;
        if (pos >= inode->i_size) {
                return 0;
        }
        blk = (pos + 1) / UX_BSIZE;
        blk = uip->i_addr[blk];
        bh = sb_bread(inode->i_sb, blk);
        udir = (struct ux_dirent *)(bh->b_data + pos % UX_BSIZE);

        /*
         * Skip over 'null' directory entries.
         */

        if (udir->d_ino == 0) {
                filp->f_pos += sizeof(struct ux_dirent);
                brelse(bh);
                goto start_again;
        } else {
                filldir(dirent, udir->d_name, 
                        sizeof(udir->d_name), pos, 
                        udir->d_ino, DT_UNKNOWN);
        }
        filp->f_pos += sizeof(struct ux_dirent);
        brelse(bh);
        return 0;        
}

struct file_operations ux_dir_operations = {
        read:                generic_read_dir,
        readdir:             ux_readdir,
        fsync:               file_fsync,
};

/*
 * When we reach this point, ux_lookup() has already been called
 * to create a negative entry in the dcache. Thus, we need to
 * allocate a new inode on disk and associate it with the dentry.
 */

int
ux_create(struct inode *dip, struct dentry *dentry, int mode)
{
        struct ux_inode                *nip;
        struct super_block        *sb = dip->i_sb;
        struct inode                *inode;
        ino_t                        inum = 0;

        /*
         * See if the entry exists. If not, create a new 
         * disk inode, and incore inode. The add the new 
         * entry to the directory.
         */

        inum = ux_find_entry(dip, (char *)dentry->d_name.name);
        if (inum) {
                return -EEXIST;
        }
        inode = new_inode(sb);
        if (!inode) {
                return -ENOSPC;
        }
        inum = ux_ialloc(sb);
        if (!inum) {
                iput(inode);
                return -ENOSPC;
        }
        ux_diradd(dip, (char *)dentry->d_name.name, inum);

        /*
         * Increment the parent link count and intialize the inode.
         */

        dip->i_nlink++;
        inode->i_uid = current->fsuid;
        inode->i_gid = (dip->i_mode & S_ISGID) ?
                        dip->i_gid : current->fsgid;
        inode->i_mtime = inode->i_atime =
                         inode->i_ctime = CURRENT_TIME;
        inode->i_blocks = inode->i_blksize = 0;
        inode->i_op = &ux_file_inops;
        inode->i_fop = &ux_file_operations;
        inode->i_mapping->a_ops = &ux_aops;
        inode->i_mode = mode;
        inode->i_nlink = 1;
        inode->i_ino = inum;
        insert_inode_hash(inode); 

        nip = (struct ux_inode *)&inode->i_private;
        nip->i_mode = mode;
        nip->i_nlink = 1;
        nip->i_atime = nip->i_ctime = nip->i_mtime = CURRENT_TIME;
        nip->i_uid = inode->i_gid;
        nip->i_gid = inode->i_gid;
        nip->i_size = 0;
        nip->i_blocks = 0;
        memset(nip->i_addr, 0, UX_DIRECT_BLOCKS);

        d_instantiate(dentry, inode);
        mark_inode_dirty(dip);
        mark_inode_dirty(inode);
        return 0;
}

/*
 * Make a new directory. We already have a negative dentry
 * so must create the directory and instantiate it.
 */

int
ux_mkdir(struct inode *dip, struct dentry *dentry, int mode)
{
        struct ux_inode         *nip;
        struct buffer_head      *bh;
        struct super_block      *sb = dip->i_sb;
        struct ux_dirent        *dirent;
        struct inode            *inode;
        ino_t                   inum = 0;
        int                     blk;

        /*
         * Make sure there isn't already an entry. If not, 
         * allocate one, a new inode and new incore inode.
         */

        inum = ux_find_entry(dip, (char *)dentry->d_name.name);
        if (inum) {
                return -EEXIST;
        }
        inode = new_inode(sb);
        if (!inode) {
                return -ENOSPC;
        }
        inum = ux_ialloc(sb);
        if (!inum) {
                iput(inode);
                return -ENOSPC;
        }
        ux_diradd(dip, (char *)dentry->d_name.name, inum);

        inode->i_uid = current->fsuid;
        inode->i_gid = (dip->i_mode & S_ISGID) ? 
                        dip->i_gid : current->fsgid;
        inode->i_mtime = inode->i_atime = 
                        inode->i_ctime = CURRENT_TIME;
        inode->i_blocks = 1;
        inode->i_blksize = UX_BSIZE;
        inode->i_op = &ux_dir_inops;
        inode->i_fop = &ux_dir_operations;
        inode->i_mapping->a_ops = &ux_aops;
        inode->i_mode = mode | S_IFDIR;
        inode->i_ino = inum;
        inode->i_size = UX_BSIZE;
        inode->i_nlink = 2;

        nip = (struct ux_inode *)&inode->i_private;
        nip->i_mode = mode | S_IFDIR;
        nip->i_nlink = 2;
        nip->i_atime = nip->i_ctime 
                     = nip->i_mtime = CURRENT_TIME;
        nip->i_uid = current->fsuid;
        nip->i_gid = (dip->i_mode & S_ISGID) ?
                      dip->i_gid : current->fsgid;
        nip->i_size = 512;
        nip->i_blocks = 1;
        memset(nip->i_addr, 0, 16);

        blk = ux_block_alloc(sb);
        nip->i_addr[0] = blk;
        bh = sb_bread(sb, blk);
        memset(bh->b_data, 0, UX_BSIZE);
        dirent = (struct ux_dirent *)bh->b_data;
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

        dip->i_nlink++;
        mark_inode_dirty(dip);
        return 0;
}

/*
 * Remove the specified directory.
 */

int
ux_rmdir(struct inode *dip, struct dentry *dentry)
{
        struct super_block      *sb = dip->i_sb;
        struct ux_fs            *fs = (struct ux_fs *)
                                       sb->s_private;
        struct ux_superblock    *usb = fs->u_sb;
        struct inode            *inode = dentry->d_inode;
        struct ux_inode         *uip = (struct ux_inode *)
                                        &inode->i_private;
        int                     inum, i;

        if (inode->i_nlink > 2) {
                return -ENOTEMPTY;
        }

        /*
         * Remove the entry from the parent directory
         */

        inum = ux_find_entry(dip, (char *)dentry->d_name.name);
        if (!inum) {
                return -ENOTDIR;
        }
        ux_dirdel(dip, (char *)dentry->d_name.name);

        /*
         * Clean up the inode
         */

        for (i=0 ; i<UX_DIRECT_BLOCKS ; i++) {
                if (uip->i_addr[i] != 0) {
                        usb->s_block[uip->i_addr[i]]
                                     = UX_BLOCK_FREE;
                        usb->s_nbfree++;
                }
        }

        /*
         * Update the superblock summaries.
         */

        usb->s_inode[dip->i_ino] = UX_INODE_FREE;
        usb->s_nifree++;
        return 0;
}

/*
 * Lookup the specified file. A call is made to iget() to
 * bring the inode into core.
 */

struct dentry *
ux_lookup(struct inode *dip, struct dentry *dentry)
{
        struct ux_inode     *uip = (struct ux_inode *)
                                    &dip->i_private;
        struct ux_dirent    dirent;
        struct inode        *inode = NULL;
        int                 inum;

        if (dentry->d_name.len > UX_NAMELEN) {
                return ERR_PTR(-ENAMETOOLONG);
        }

        inum = ux_find_entry(dip, (char *)dentry->d_name.name);
        if (inum) {
                inode = iget(dip->i_sb, inum);
                if (!inode) {
                        return ERR_PTR(-EACCES);
                }
        }
        d_add(dentry, inode);
        return NULL;
}

/*
 * Called in response to an ln command/syscall.
 */

int
ux_link(struct dentry *old, struct inode *dip, struct dentry *new)
{
        struct inode       *inode = old->d_inode;
        int                error;

        /*
         * Add the new file (new) to its parent directory (dip)
         */

        error = ux_diradd(dip, new->d_name.name, inode->i_ino);

        /*
         * Increment the link count of the target inode
         */

        inode->i_nlink++;
        mark_inode_dirty(inode);
        atomic_inc(&inode->i_count);
        d_instantiate(new, inode);
        return 0;
}

/*
 * Called to remove a file (decrement its link count)
 */

int
ux_unlink(struct inode *dip, struct dentry *dentry)
{
        struct inode       *inode = dentry->d_inode;

        ux_dirdel(dip, (char *)dentry->d_name.name);
        inode->i_nlink--;
        mark_inode_dirty(inode);
        return 0;
}

struct inode_operations ux_dir_inops = {
        create:              ux_create,
        lookup:              ux_lookup,
        mkdir:               ux_mkdir,
        rmdir:               ux_rmdir,
        link:                ux_link,
        unlink:              ux_unlink,
};

