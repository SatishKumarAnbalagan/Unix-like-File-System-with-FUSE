/*
 * file:        homework.c
 * description: skeleton file for CS 5600 system
 *
 * CS 5600, Computer Systems, Northeastern CCIS
 * Peter Desnoyers, November 2019
 */

#define FUSE_USE_VERSION 27
#define _FILE_OFFSET_BITS 64

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fuse.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "fs5600.h"

/* if you don't understand why you can't use these system calls here, 
 * you need to read the assignment description another time
 */
#define stat(a,b) error do not use stat()
#define open(a,b) error do not use open()
#define read(a,b,c) error do not use read()
#define write(a,b,c) error do not use write()
#define FILENAME_MAXLENGTH 32

/* disk access. All access is in terms of 4KB blocks; read and
 * write functions return 0 (success) or -EIO.
 */
extern int block_read(void *buf, int lba, int nblks);
extern int block_write(void *buf, int lba, int nblks);

/* global variables */
struct fs_super superblock;
struct fs_inode *inode_region;	/* inodes in memory */


/* function declaration */

/* truncate the last token from path return 1 if succeed, 0 if not*/
int truncate_path (const char *path, char **truncated_path);

/* translate: return the inode number of given path */
static int translate(const char *path);

/* bitmap functions
 */
void bit_set(unsigned char *map, int i)
{
    map[i/8] |= (1 << (i%8));
}
void bit_clear(unsigned char *map, int i)
{
    map[i/8] &= ~(1 << (i%8));
}
int bit_test(unsigned char *map, int i)
{
    return map[i/8] & (1 << (i%8));
}


/* init - this is called once by the FUSE framework at startup. Ignore
 * the 'conn' argument.
 * recommended actions:
 *   - read superblock
 *   - allocate memory, read bitmaps and inodes
 */
void* fs_init(struct fuse_conn_info *conn)
{
    /* your code here */
    struct fs_super sb;

    return NULL;
}

/* Note on path translation errors:
 * In addition to the method-specific errors listed below, almost
 * every method can return one of the following errors if it fails to
 * locate a file or directory corresponding to a specified path.
 *
 * ENOENT - a component of the path doesn't exist.
 * ENOTDIR - an intermediate component of the path (e.g. 'b' in
 *           /a/b/c) is not a directory
 */

/* note on splitting the 'path' variable:
 * the value passed in by the FUSE framework is declared as 'const',
 * which means you can't modify it. The standard mechanisms for
 * splitting strings in C (strtok, strsep) modify the string in place,
 * so you have to copy the string and then free the copy when you're
 * done. One way of doing this:
 *
 *    char *_path = strdup(path);
 *    int inum = translate(_path);
 *    free(_path);
 */

static int translate(const char *path) {
    /* split the path */
    char *_path;
    _path = strdup(path);
    /* traverse to path */
    /* root father_inode */
    int inode_num = 1;
    struct fs_inode *father_inode;
    struct fs_dirent *dir;
    dir = malloc(FS_BLOCK_SIZE);

    struct fs_dirent dummy_dir = {
	.valid = 1,
	.inode = inode_num,
	.name = "/",
    };
    struct fs_dirent *current_dir = &dummy_dir;

    char *token;
    char *delim = "/";
    token = strtok(_path, delim);
    int error = 0;
    /* traverse all the subsides */
    /* if found, return corresponding father_inode */
    /* else, return error */
    while (token != NULL) {
        if (current_dir->valid == 0) {
	        error = -ENOENT;
            break;
	    }

	    father_inode = &inode_region[inode_num];
	    int block_pos = father_inode->ptrs[0];
        //block_read(dir, block_pos, 1)
	    int i;
	    int found = 0;
	    for (i = 0; i < 32; i++) {
            if (strcmp(dir[i].name, token) == 0 && dir[i].valid == 1) {
                found = 1;
                inode_num = dir[i].inode;
                current_dir = &dir[i];
            }
	    }
	    if (found == 0) {
            error = -ENOENT;
            break;
	    }
        token = strtok(NULL, delim);
    }

    free(dir);
    free(_path);
    if (error != 0) {
        return error;
    }
    return inode_num;
}

int truncate_path (const char *path, char **truncated_path) {
    int i = strlen(path) - 1;
    // strip the tailling '/'
    // deal with '///' case
    for (; i >= 0; i--) {
        if (path[i] != '/') {
            break;
        }
    }
    for (; i >= 0; i--) {
    	if (path[i] == '/') {
            *truncated_path = (char*)malloc(sizeof(char) * (i + 2));
            memcpy(*truncated_path, path, i + 1);
            (*truncated_path)[i + 1] = '\0';
            return 1;
    	}
    }
    return 0;
}

/* setattr - set file or directory attributes. */
static void set_attr(struct fs_inode inode, struct stat *sb) {
    /* set every other bit to zero */
    memset(sb, 0, sizeof(struct stat));
    sb->st_mode = inode.mode;
    sb->st_uid = inode.uid;
    sb->st_gid = inode.gid;
    sb->st_size = inode.size;
    sb->st_blocks = (inode.size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
    sb->st_nlink = 1;
    sb->st_atime = inode.mtime;
    sb->st_ctime = inode.ctime;
    sb->st_mtime = inode.mtime;
}

/* getattr - get file or directory attributes. For a description of
 *  the fields in 'struct stat', see 'man lstat'.
 *
 * Note - for several fields in 'struct stat' there is no corresponding
 *  information in our file system:
 *    st_nlink - always set it to 1
 *    st_atime, st_ctime - set to same value as st_mtime
 *
 * success - return 0
 * errors - path translation, ENOENT
 * hint - factor out inode-to-struct stat conversion - you'll use it
 *        again in readdir
 */
int fs_getattr(const char *path, struct stat *sb)
{
    /* your code here */
    fs_init(NULL);
    int inum = translate(path);
    if (inum == -ENOENT || inum == -ENOTDIR || inum == -EOPNOTSUPP) {
    	return inum;
    }

    struct fs_inode inode = inode_region[inum];
    set_attr(inode, sb);

    return 0;
}

/* readdir - get directory contents.
 *
 * call the 'filler' function once for each valid entry in the 
 * directory, as follows:
 *     filler(buf, <name>, <statbuf>, 0)
 * where <statbuf> is a pointer to a struct stat
 * success - return 0
 * errors - path resolution, ENOTDIR, ENOENT
 * 
 * hint - check the testing instructions if you don't understand how
 *        to call the filler function
 */
int fs_readdir(const char *path, void *ptr, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
    /* your code here */
    return -EOPNOTSUPP;
}

/* create - create a new file with specified permissions
 *
 * success - return 0
 * errors - path resolution, EEXIST
 *          in particular, for create("/a/b/c") to succeed,
 *          "/a/b" must exist, and "/a/b/c" must not.
 *
 * Note that 'mode' will already have the S_IFREG bit set, so you can
 * just use it directly. Ignore the third parameter.
 *
 * If a file or directory of this name already exists, return -EEXIST.
 * If there are already 128 entries in the directory (i.e. it's filled an
 * entire block), you are free to return -ENOSPC instead of expanding it.
 */
int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    /* your code here */
    return -EOPNOTSUPP;
}

/* mkdir - create a directory with the given mode.
 *
 * WARNING: unlike fs_create, @mode only has the permission bits. You
 * have to OR it with S_IFDIR before setting the inode 'mode' field.
 *
 * success - return 0
 * Errors - path resolution, EEXIST
 * Conditions for EEXIST are the same as for create. 
 */ 
int fs_mkdir(const char *path, mode_t mode)
{
    /* your code here */
    return -EOPNOTSUPP;
}


/* unlink - delete a file
 *  success - return 0
 *  errors - path resolution, ENOENT, EISDIR
 */
int fs_unlink(const char *path)
{
    /* your code here */
    return -EOPNOTSUPP;
}

/* rmdir - remove a directory
 *  success - return 0
 *  Errors - path resolution, ENOENT, ENOTDIR, ENOTEMPTY
 */
int fs_rmdir(const char *path)
{
    /* your code here */
    return -EOPNOTSUPP;
}

/* rename - rename a file or directory
 * success - return 0
 * Errors - path resolution, ENOENT, EINVAL, EEXIST
 *
 * ENOENT - source does not exist
 * EEXIST - destination already exists
 * EINVAL - source and destination are not in the same directory
 *
 * Note that this is a simplified version of the UNIX rename
 * functionality - see 'man 2 rename' for full semantics. In
 * particular, the full version can move across directories, replace a
 * destination file, and replace an empty directory with a full one.
 */
int fs_rename(const char *src_path, const char *dst_path)
{
    /* your code here */
    return -EOPNOTSUPP;
}

/* chmod - change file permissions
 * utime - change access and modification times
 *         (for definition of 'struct utimebuf', see 'man utime')
 *
 * success - return 0
 * Errors - path resolution, ENOENT.
 */
int fs_chmod(const char *path, mode_t mode)
{
    /* your code here */
    return -EOPNOTSUPP;
}

int fs_utime(const char *path, struct utimbuf *ut)
{
    /* your code here */
    return -EOPNOTSUPP;
}

/* truncate - truncate file to exactly 'len' bytes
 * success - return 0
 * Errors - path resolution, ENOENT, EISDIR, EINVAL
 *    return EINVAL if len > 0.
 */
int fs_truncate(const char *path, off_t len)
{
    /* you can cheat by only implementing this for the case of len==0,
     * and an error otherwise.
     */
    if (len != 0)
	return -EINVAL;		/* invalid argument */

    /* your code here */
    return -EOPNOTSUPP;
}


/* read - read data from an open file.
 * success: should return exactly the number of bytes requested, except:
 *   - if offset >= file len, return 0
 *   - if offset+len > file len, return #bytes from offset to end
 *   - on error, return <0
 * Errors - path resolution, ENOENT, EISDIR
 */
int fs_read(const char *path, char *buf, size_t len, off_t offset,
	    struct fuse_file_info *fi)
{
    /* your code here */
    return -EOPNOTSUPP;
}

/* write - write data to a file
 * success - return number of bytes written. (this will be the same as
 *           the number requested, or else it's an error)
 * Errors - path resolution, ENOENT, EISDIR
 *  return EINVAL if 'offset' is greater than current file length.
 *  (POSIX semantics support the creation of files with "holes" in them, 
 *   but we don't)
 */
int fs_write(const char *path, const char *buf, size_t len,
	     off_t offset, struct fuse_file_info *fi)
{
    /* your code here */
    return -EOPNOTSUPP;
}

/* statfs - get file system statistics
 * see 'man 2 statfs' for description of 'struct statvfs'.
 * Errors - none. Needs to work.
 */
int fs_statfs(const char *path, struct statvfs *st)
{
    /* needs to return the following fields (set others to zero):
     *   f_bsize = BLOCK_SIZE
     *   f_blocks = total image - (superblock + block map)
     *   f_bfree = f_blocks - blocks used
     *   f_bavail = f_bfree
     *   f_namemax = <whatever your max namelength is>
     *
     * it's OK to calculate this dynamically on the rare occasions
     * when this function is called.
     */
    /* your code here */
    st->f_bsize = FS_BLOCK_SIZE;
    int block_map = sizeof(superblock.pad);
    st->f_blocks = superblock.disk_size - (1 + block_map);
    
    int i = 0, num_free_blocks = 0;
    
    for (i = 0; i < superblock.disk_size; i++) {
            num_free_blocks++;
    }

    st->f_bfree = num_free_blocks;
    st->f_bavail = num_free_blocks;
    st->f_namemax = FILENAME_MAXLENGTH;

    return -EOPNOTSUPP;
}

/* operations vector. Please don't rename it, or else you'll break things
 */
struct fuse_operations fs_ops = {
    .init = fs_init,            /* read-mostly operations */
    .getattr = fs_getattr,
    .readdir = fs_readdir,
    .rename = fs_rename,
    .chmod = fs_chmod,
    .read = fs_read,
    .statfs = fs_statfs,

    .create = fs_create,        /* write operations */
    .mkdir = fs_mkdir,
    .unlink = fs_unlink,
    .rmdir = fs_rmdir,
    .utime = fs_utime,
    .truncate = fs_truncate,
    .write = fs_write,
};

