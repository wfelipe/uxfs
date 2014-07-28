/*--------------------------------------------------------------*/
/*---------------------------- mkfs.c --------------------------*/
/*--------------------------------------------------------------*/

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <linux/fs.h>
#include <sys/stat.h>
#include <linux/types.h>
#include "../kern/uxfs.h"

int main(int argc, char **argv)
{
	struct uxfs_dirent dir;
	struct uxfs_superblock sb;
	struct uxfs_inode inode;
	time_t tm;
	off_t nsectors = UXFS_MAXBLOCKS;
	int devfd, error, i;
	char block[UXFS_BSIZE];

	if (argc != 2) {
		fprintf(stderr, "uxmkfs: Need to specify device\n");
		exit(1);
	}
	devfd = open(argv[1], O_WRONLY);
	if (devfd < 0) {
		fprintf(stderr, "uxmkfs: Failed to open device\n");
		exit(1);
	}
	error = lseek(devfd, (off_t) (nsectors * 512), SEEK_SET);
	if (error == -1) {
		fprintf(stderr, "uxmkfs: Cannot create filesystem"
			" of specified size\n");
		exit(1);
	}
	lseek(devfd, 0, SEEK_SET);

	/*added to initialize every block on the device to 0 before writing anything to the device*/
	for(i=0; i<UXFS_BSIZE; i++){
	  block[i]=0;
	}
	for(i=0; i<UXFS_MAXBLOCKS; i++){
	  write(devfd, block, UXFS_BSIZE );
	}

	lseek(devfd, 0, SEEK_SET);

	/*
	 * Fill in the fields of the superblock and write
	 * it out to the first block of the device.
	 */

	sb.s_magic = UXFS_MAGIC;
	sb.s_mod = UXFS_FSCLEAN;
	sb.s_nifree = UXFS_MAXFILES - 4;
	sb.s_nbfree = UXFS_MAXBLOCKS - 2;

	/*
	 * First 4 inodes are in use. Inodes 0 and 1 are not
	 * used by anything, 2 is the root directory and 3 is
	 * lost+found.
	 */

	sb.s_inode[0] = UXFS_INODE_INUSE;
	sb.s_inode[1] = UXFS_INODE_INUSE;
	sb.s_inode[2] = UXFS_INODE_INUSE;
	sb.s_inode[3] = UXFS_INODE_INUSE;

	/*
	 * The rest of the inodes are marked unused
	 */

	for (i = 4; i < UXFS_MAXFILES; i++)
		sb.s_inode[i] = UXFS_INODE_FREE;

	/*
	 * The first two blocks are allocated for the entries
	 * for the root and lost+found directories.
	 */

	sb.s_block[0] = UXFS_BLOCK_INUSE;
	sb.s_block[1] = UXFS_BLOCK_INUSE;

	/*
	 * The rest of the blocks are marked unused
	 */

	for (i = 2; i < UXFS_MAXBLOCKS; i++)
		sb.s_block[i] = UXFS_BLOCK_FREE;

	write(devfd, (char *)&sb, sizeof(struct uxfs_superblock));

	/*
	 * The root directory and lost+found directory inodes
	 * must be initialized.
	 */

	time(&tm);
	memset((void *)&inode, 0, sizeof(struct uxfs_inode));
	inode.i_mode = S_IFDIR | 0755;
	inode.i_nlink = 3;	/* ".", ".." and "lost+found" */
	inode.i_atime = tm;
	inode.i_mtime = tm;
	inode.i_ctime = tm;
	inode.i_uid = 0;
	inode.i_gid = 0;
	inode.i_size = UXFS_BSIZE;
	inode.i_blocks = 1;
	inode.i_addr[0] = UXFS_FIRST_DATA_BLOCK;

	lseek(devfd, (UXFS_INODE_BLOCK + UXFS_ROOT_INO)* UXFS_BSIZE, SEEK_SET);
	write(devfd, (char *)&inode, sizeof(struct uxfs_inode));

	memset((void *)&inode, 0, sizeof(struct uxfs_inode));
	inode.i_mode = S_IFDIR | 0755;
	inode.i_nlink = 2;	/* "." and ".." */
	inode.i_atime = tm;
	inode.i_mtime = tm;
	inode.i_ctime = tm;
	inode.i_uid = 0;
	inode.i_gid = 0;
	inode.i_size = UXFS_BSIZE;
	inode.i_blocks = 1;
	inode.i_addr[0] = UXFS_FIRST_DATA_BLOCK + 1;

	lseek(devfd, (UXFS_INODE_BLOCK + UXFS_ROOT_INO + 1) * UXFS_BSIZE, SEEK_SET);
	write(devfd, (char *)&inode, sizeof(struct uxfs_inode));

	/*
	 * Fill in the directory entries for root 
	 */

	lseek(devfd, UXFS_FIRST_DATA_BLOCK * UXFS_BSIZE, SEEK_SET);
	memset((void *)&block, 0, UXFS_BSIZE);
	write(devfd, block, UXFS_BSIZE);
	lseek(devfd, UXFS_FIRST_DATA_BLOCK * UXFS_BSIZE, SEEK_SET);
	dir.d_ino = 2;
	strcpy(dir.d_name, ".");
	write(devfd, (char *)&dir, sizeof(struct uxfs_dirent));
	dir.d_ino = 2;
	strcpy(dir.d_name, "..");
	write(devfd, (char *)&dir, sizeof(struct uxfs_dirent));
	dir.d_ino = 3;
	strcpy(dir.d_name, "lost+found");
	write(devfd, (char *)&dir, sizeof(struct uxfs_dirent));

	/*
	 * Fill in the directory entries for lost+found 
	 */

	lseek(devfd, UXFS_FIRST_DATA_BLOCK * UXFS_BSIZE + UXFS_BSIZE, SEEK_SET);
	memset((void *)&block, 0, UXFS_BSIZE);
	write(devfd, block, UXFS_BSIZE);
	lseek(devfd, UXFS_FIRST_DATA_BLOCK * UXFS_BSIZE + UXFS_BSIZE, SEEK_SET);
	dir.d_ino = 3; //THIS IS INODE 3, NOT 2
	strcpy(dir.d_name, ".");
	write(devfd, (char *)&dir, sizeof(struct uxfs_dirent));
	dir.d_ino = 2;
	strcpy(dir.d_name, "..");
	write(devfd, (char *)&dir, sizeof(struct uxfs_dirent));

	return 0;
}
