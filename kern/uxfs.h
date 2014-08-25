/*--------------------------------------------------------------*/
/*---------------------------- uxfs.h -------------------------*/
/*--------------------------------------------------------------*/

extern struct address_space_operations uxfs_aops;
extern struct inode_operations uxfs_file_inops;
extern struct inode_operations uxfs_dir_inops;
extern struct file_operations uxfs_dir_operations;
extern struct file_operations uxfs_file_operations;

#define UXFS_NAMELEN		28
#define UXFS_DIRS_PER_BLOCK	16
#define UXFS_DIRECT_BLOCKS	16
#define UXFS_MAXFILES		32
#define UXFS_MAXBLOCKS		460	//changed to 460 so the superblock is not bigger than a block
#define UXFS_FIRST_DATA_BLOCK	50
#define UXFS_BSIZE		512
#define UXFS_BSIZE_BITS		9
#define UXFS_MAGIC		0x58494e55	// UNIX
#define UXFS_INODE_BLOCK		8
#define UXFS_ROOT_INO		2

//#define s_private     u.generic_sbp
//#define i_private     u.generic_ip

/*
 * The on-disk superblock. The number of inodes and 
 * data blocks is fixed.
 */

struct uxfs_superblock {	//made smaller using chars so it fits in a block
	__u32 s_magic;
	__u32 s_mod;
	__u32 s_nifree;
	char s_inode[UXFS_MAXFILES];	//changed to char from __u32
	__u32 s_nbfree;
	char s_block[UXFS_MAXBLOCKS];	//changed to char from __u32
};

/*
 * The on-disk inode.
 */

struct uxfs_inode {
	__u32 i_mode;
	__u32 i_nlink;
	__u32 i_atime;
	__u32 i_mtime;
	__u32 i_ctime;
	__u32 i_uid;
	__u32 i_gid;
	__u32 i_size;
	__u32 i_blocks;
	__u32 i_addr[UXFS_DIRECT_BLOCKS];
};

/*
 * the actual inode allocation
 */

struct uxfs_inode_info {
	struct uxfs_inode uip;
#ifdef __KERNEL__
	struct inode vfs_inode;
#endif
};

/*
 * Allocation flags
 */

#define UXFS_INODE_FREE	0
#define UXFS_INODE_INUSE	1
#define UXFS_BLOCK_FREE	0
#define UXFS_BLOCK_INUSE	1

/*
 * Filesystem flags
 */

#define UXFS_FSCLEAN	0
#define UXFS_FSDIRTY	1

/*
 * FIxed size directory entry.
 */

struct uxfs_dirent {
	__u32 d_ino;
	char d_name[UXFS_NAMELEN];
};

/*
 * Used to hold filesystem information in-core permanently.
 */

struct uxfs_fs {
	struct uxfs_superblock *u_sb;
	struct buffer_head *u_sbh;
};

#ifdef __KERNEL__

extern ino_t uxfs_ialloc(struct super_block *);
extern int uxfs_find_entry(struct inode *, char *);
__u32 uxfs_block_alloc(struct super_block *);
extern __u32 uxfs_block_alloc(struct super_block *);
extern int uxfs_unlink(struct inode *, struct dentry *);
extern int uxfs_link(struct dentry *, struct inode *, struct dentry *);
struct inode *uxfs_iget(struct super_block *, unsigned long);

static inline struct uxfs_inode_info *uxfs_i(struct inode *inode)
{
	return container_of(inode, struct uxfs_inode_info, vfs_inode);
}

#endif
