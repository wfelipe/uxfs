/*--------------------------------------------------------------*/
/*---------------------------- fsdb.c --------------------------*/
/*--------------------------------------------------------------*/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <linux/fs.h>
#include "../kern/ux_fs.h"

struct ux_superblock       sb;
int                        devfd;

void
print_inode(int inum, struct ux_inode *uip)
{
        char                    buf[UX_BSIZE];
        struct ux_dirent        *dirent;
        int                     i, x;

        printf("\ninode number %d\n", inum);
        printf("  i_mode     = %x\n", uip->i_mode);
        printf("  i_nlink    = %d\n", uip->i_nlink);
        printf("  i_atime    = %s",
               ctime((time_t *)&uip->i_atime));
        printf("  i_mtime    = %s", 
               ctime((time_t *)&uip->i_mtime));
        printf("  i_ctime    = %s", 
               ctime((time_t *)&uip->i_ctime));
        printf("  i_uid      = %d\n", uip->i_uid);
        printf("  i_gid      = %d\n", uip->i_gid);
        printf("  i_size     = %d\n", uip->i_size);
        printf("  i_blocks   = %d", uip->i_blocks);
        for (i=0 ; i<UX_DIRECT_BLOCKS; i++) {
                if (i % 4 == 0) {
                        printf("\n");
                }
                printf("  i_addr[%2d] = %3d ", 
                       i, uip->i_addr[i]);
        }

        /*
         * Print out the directory entries
         */

        if (uip->i_mode & S_IFDIR) {
                printf("\n\n  Directory entries:\n");
                for (i=0 ; i < uip->i_blocks ; i++) {
                        lseek(devfd, uip->i_addr[i] * UX_BSIZE, 
                              SEEK_SET);
                        read(devfd, buf, UX_BSIZE);
                        dirent = (struct ux_dirent *)buf;
                        for (x = 0 ; x < UX_DIRECT_BLOCKS ; x++) {
                                if (dirent->d_ino != 0) {
                                        printf("    inum[%2d],"
                                               "name[%s]\n",
                                               dirent->d_ino, 
                                               dirent->d_name);
                                } 
                                dirent++;
                        }
                }
                printf("\n");
        } else {
                printf("\n\n");
        }
}

int read_inode(ino_t inum, struct ux_inode *uip)
{
        if (sb.s_inode[inum] == UX_INODE_FREE) {
                return -1;
        }
        lseek(devfd, (UX_INODE_BLOCK * UX_BSIZE) + 
              (inum * UX_BSIZE), SEEK_SET);
        read(devfd, (char *)uip, sizeof(struct ux_inode));
        return 0;
}

main(int argc, char **argv)
{
        struct ux_inode           inode;
        char                      buf[512];
        char                      command[512];
        off_t                     nsectors;
        int                       error, i, blk;
        ino_t                     inum;

        devfd = open(argv[1], O_RDWR);
        if (devfd < 0) {
                fprintf(stderr, "uxmkfs: Failed to open device\n");
                exit(1);
        }

        /*
         * Read in and validate the superblock
         */

        read(devfd, (char *)&sb, sizeof(struct ux_superblock));
        if (sb.s_magic != UX_MAGIC) {
                printf("This is not a uxfs filesystem\n");
                exit(1);
        }

        while (1) {
                printf("uxfsdb > ") ;
                fflush(stdout);
                scanf("%s", command);
                if (command[0] == 'q') {
                        exit(0);
                }
                if (command[0] == 'i') {
                        inum = atoi(&command[1]);
                        read_inode(inum, &inode);
                        print_inode(inum, &inode);
                }
                if (command[0] == 's') {
                        printf("\nSuperblock contents:\n");
                        printf("  s_magic   = 0x%x\n", sb.s_magic);
                        printf("  s_mod     = %s\n",
                               (sb.s_mod == UX_FSCLEAN) ?
                               "UX_FSCLEAN" : "UX_FSDIRTY");
                        printf("  s_nifree  = %d\n", sb.s_nifree);
                        printf("  s_nbfree  = %d\n\n", sb.s_nbfree);
                }
        }
}

