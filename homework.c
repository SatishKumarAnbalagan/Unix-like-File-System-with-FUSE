/*
 * file:        homework.c
 * description: skeleton file for CS 5600 system
 *
 * CS 5600, Computer Systems, Northeastern CCIS
 * Peter Desnoyers, November 2019
 */

#define FUSE_USE_VERSION 27
#define _FILE_OFFSET_BITS 64

#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "fs5600.h"

/* if you don't understand why you can't use these system calls here,
 * you need to read the assignment description another time
 */
#define stat(a, b) error do not use stat()
#define open(a, b) error do not use open()
#define read(a, b, c) error do not use read()
#define write(a, b, c) error do not use write()
#define FILENAME_MAXLENGTH 32

#define MAX_PATH_LEN 10
#define MAX_NAME_LEN 27
#define MAX_DIREN_NUM 128

/* disk access. All access is in terms of 4KB blocks; read and
 * write functions return 0 (success) or -EIO.
 */
extern int block_read(void *buf, int lba, int nblks);
extern int block_write(void *buf, int lba, int nblks);

/* global variables */
struct fs_super superblock;
unsigned char bitmap[FS_BLOCK_SIZE];

struct fs_inode *inode_region; /* inodes in memory */

/* function declaration */

/* truncate the last token from path return 1 if succeed, 0 if not*/
int truncate_path(const char *path, char **truncated_path);

/* translate: return the inode number of given path */
static int translate(const char *path);

/* bitmap functions */
void bit_set(unsigned char *map, int i) { map[i / 8] |= (1 << (i % 8)); }
void bit_clear(unsigned char *map, int i) { map[i / 8] &= ~(1 << (i % 8)); }
int bit_test(unsigned char *map, int i) { return map[i / 8] & (1 << (i % 8)); }

/* init - this is called once by the FUSE framework at startup. Ignore
 * the 'conn' argument.
 * recommended actions:
 *   - read superblock
 *   - allocate memory, read bitmaps and inodes
 */
void *fs_init(struct fuse_conn_info *conn) {
    /* your code here */
    /* here 1 stands for block size, here is 4096 bytes */
    block_read(&superblock, 0, 1);

    /* read bitmaps */
    block_read(&bitmap, 1, 1);

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

static int parse_path(const char *path, char **pathv) {
    // char * token = strtok(path, "/");
    // int count = 0;
    // while (token != NULL) {
    //     pathv[count] = token;
    //     count++;
    //     token = strtok(NULL, "/");
    // }
    // return count;

    int i;
    for (i = 0; i < MAX_PATH_LEN; i++) {
        if ((pathv[i] = strtok((char*) path, "/")) == NULL) break;
        if (strlen(pathv[i]) > MAX_NAME_LEN)
            pathv[i][MAX_NAME_LEN] = 0;  // truncate to 27 characters
        path = NULL;
    }
    return i;
}

static int translate(const char *path) {
    char *pathv[10];
    int pathc = parse_path(path, pathv);
    int inum = 2;  // root inode
    for (int i = 0; i < pathc; i++) {
        struct fs_inode _in;
        block_read(&_in, inum, 1);
        if (!S_ISDIR(_in.mode)) {
            return -ENOTDIR;
        }
        int blknum = _in.ptrs[0];  // ptrs are a list of block numbers
        struct fs_dirent des[MAX_DIREN_NUM];
        block_read(des, blknum, 1);
        int entry_found = 0;
        for (int j = 0; j < MAX_DIREN_NUM; j++) {
            if (des[j].valid && strcmp(des[j].name, pathv[i]) == 0) {
                inum = des[j].inode;
                entry_found = 1;
                break;
            }
        }
        if (!entry_found) return -ENOENT;
    }
    return inum;
}

static int find_enclosing(char *path) {
    char *pathv[10];
    int pathc = parse_path(path, pathv);
    // enclosing node of root node isn't defined.
    if (pathc == 0) return -ENOTSUP;
    int inum = 2;  // root inode

    for (int i = 0; i < pathc - 1; i++) {
        struct fs_inode _in;
        block_read(&_in, inum, 1);
        if (!S_ISDIR(_in.mode)) {
            return -ENOTDIR;
        }
        int blknum = _in.ptrs[0];  // ptrs are a list of block numbers
        struct fs_dirent des[MAX_DIREN_NUM];
        block_read(des, blknum, 1);
        int entry_found = 0;
        for (int j = 0; j < MAX_DIREN_NUM; j++) {
            if (des[j].valid && strcmp(des[j].name, pathv[i]) == 0) {
                inum = des[j].inode;
                entry_found = 1;
                break;
            }
        }
        if (!entry_found) return -ENOENT;
    }
    return inum;
}

int truncate_path(const char *path, char **truncated_path) {
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
            *truncated_path = (char *)malloc(sizeof(char) * (i + 2));
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
    sb->st_blksize = FS_BLOCK_SIZE;
    sb->st_blocks = (inode.size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
    sb->st_nlink = 1;
    sb->st_atime = inode.mtime;
    sb->st_ctime = inode.ctime;
    sb->st_mtime = inode.mtime;
}

/* check whether this inode is a directory */
int inode_is_dir(int father_inum, int inum) {
    struct fs_inode *inode;
    struct fs_dirent *dir;
    dir = (struct fs_dirent *)malloc(MAX_DIREN_NUM);

    inode = &inode_region[father_inum];
    int block_pos = inode->ptrs[0];
    block_write(dir, block_pos, 1);
    int i;
    for (i = 0; i < MAX_DIREN_NUM; i++) {
        if (dir[i].valid == 0) {
            continue;
        }
        if (dir[i].inode == inum) {
            int result = dir[i].inode;
            free(dir);
            return result;
        }
    }
    free(dir);
    return 0;
}

int find_free_dirent_num(struct fs_inode *inode) {
    struct fs_dirent *dir = (struct fs_dirent *)malloc(MAX_DIREN_NUM);
    int blknum = inode->ptrs[0];  // ptrs are a list of block numbers
    block_read(dir, blknum, 1);

    int no_free_dirent = -1;
    for (int i = 0; i < MAX_DIREN_NUM; i++) {
        if (!dir[i].valid) {
            no_free_dirent = i;
            break;
        }
    }
    free(dir);
    return no_free_dirent;
}

int find_free_inode_map_bit() {
    int inode_capacity = FS_BLOCK_SIZE * 8;
    for (int i = 2; i < inode_capacity; i++) {
        if (!bit_test(bitmap, i)) {
            return i;
        }
    }
    return -ENOSPC;
}

void update_bitmap() {
    block_write(bitmap, 1, 1);
    /* shld we update fs_inode as well ?
    block_write(_in, inum, 1);
    */
}

void update_inode(int inum) {
    //gotta do inode update as well
    //block_write(_in, inum, 1);
}

static char *get_name(char *path) {
    int i = strlen(path) - 1;
    for (; i >= 0; i--) {
        if (path[i] == '/') {
            i++;
            break;
        }
    }
    char *result = &path[i];
    return result;
}

static int is_in_same_directory(char *src_path, char *dst_path,
                                char *src_pathv[], int *src_pathc,
                                char *dst_pathv[], int *dst_pathc) {
    *src_pathc = parse_path(src_path, src_pathv);
    *dst_pathc = parse_path(dst_path, dst_pathv);
    if (*src_pathc != *dst_pathc) return -1;
    for (int i = 0; i < *src_pathc - 1; i++) {
        if (strcmp(src_pathv[i], dst_pathv[i]) != 0) return -1;
    }
    return 1;
}

static int exists_in_directory(struct fs_dirent des[], char *name) {
    for (int i = 0; i < MAX_DIREN_NUM; i++) {
        if (des[i].valid && strcmp(des[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int find_free_block_number() {
    for (int i = 0; i < FS_BLOCK_SIZE * 8; i++) {
        if (!bit_test(bitmap, i)) {
            int *free_blk = calloc(1, FS_BLOCK_SIZE);
            block_write(free_blk, i, 1);
            free(free_blk);
            return i;
        }
    }
    return -ENOSPC;
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
int fs_getattr(const char *path, struct stat *sb) {
    /* your code here */
    fs_init(NULL);
    char *_path = strdup(path);
    int inum = translate(_path);
    free(_path);
    if (inum == -ENOENT || inum == -ENOTDIR || inum == -EOPNOTSUPP) {
        return inum;
    }

    struct fs_inode inode;
    block_read(&inode, inum, 1);
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
               off_t offset, struct fuse_file_info *fi) {
    char *_path = strdup(path);
    int inum = translate(_path);
    free(_path);
    if (inum == -ENOTDIR || inum == -ENOENT) {
        return inum;
    }
    // if result path inode isn't directory return error.
    struct fs_inode _in;
    block_read(&_in, inum, 1);
    if (!S_ISDIR(_in.mode)) {
        return -ENOTDIR;
    }

    int blknum = _in.ptrs[0];  // ptrs are a list of block numbers

    struct fs_dirent des[MAX_DIREN_NUM];

    block_read(des, blknum, 1);
    // filler prototype
    // int test_filler(void *ptr, const char *name, const struct stat *st, off_t
    // off)
    for (int i = 0; i < MAX_DIREN_NUM; i++) {
        if (des[i].valid) {
            struct stat sb;
            struct fs_inode dir_entry_inode;
            block_read(&dir_entry_inode, des[i].inode, 1);
            set_attr(dir_entry_inode, &sb);
            filler(ptr, des[i].name, &sb, 0);
        }
    }
    return 0;
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
int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

    if (!S_ISREG(mode)) {
        return -EINVAL;
    }

    // check if parent dir exist
    char *parent_path;
    if (!truncate_path(path, &parent_path)) {
        // path is "/"
        return -1;
    }

    int inum_dir = translate(parent_path);
    if (inum_dir == -ENOENT || inum_dir == -ENOTDIR) {
        return inum_dir;
    }

    // check if file exists
    int inum = translate(path);
    if (inum > 0) {
        return -EEXIST;
    }

    struct fs_inode* parent_inode = (struct fs_inode*) malloc(sizeof(struct fs_inode));
    block_read(parent_inode, inum, 1);
    if (!S_ISDIR(parent_inode->mode)) {
        return -ENOTDIR;
    }

    int no_free_dirent = find_free_dirent_num(parent_inode);
    if(no_free_dirent < 0) {
        return -ENOSPC;
    }

    // set inode region bitmap
    time_t time_raw_format;
    time( &time_raw_format );
    struct fs_inode new_inode = {
            .uid = getuid(),
            .gid = getgid(),
            .mode = mode,
            .ctime = time_raw_format,
            .mtime = time_raw_format,
            .size = 0,
    };
    int free_inum = find_free_inode_map_bit();
    if (free_inum < 0) {
        return -ENOSPC;
    }
    bit_set(bitmap, free_inum);
    update_bitmap();

    memcpy(&inode_region[free_inum], &new_inode, sizeof(struct fs_inode));
    update_inode(free_inum);

    // set parent_inode dirent then write it
    char *_path = strdup(path);
    char *tmp_name = get_name(_path);
    struct fs_dirent new_dirent = {
            .valid = 1,
            .inode = free_inum,
            .name = "",
    };
    assert(strlen(tmp_name) < MAX_NAME_LEN);
    memcpy(new_dirent.name, tmp_name, strlen(tmp_name));
    new_dirent.name[strlen(tmp_name)] = '\0';

    struct fs_dirent *dir = (struct fs_dirent *)malloc(MAX_DIREN_NUM);
    int blknum = (parent_inode->ptrs)[0];
    block_read(dir, blknum, 1);
    memcpy(&dir[no_free_dirent], &new_dirent, sizeof(struct fs_dirent));
    block_write(dir, blknum, 1);


    free(dir);
    free(_path);
    return 0;

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
int fs_mkdir(const char *path, mode_t mode) {
    /* your code here */

    mode = mode | S_IFDIR;
    if (!S_ISDIR(mode)) {
        return -EINVAL;
    }

    // check if parent dir exist
    char *parent_path;
    if (!truncate_path(path, &parent_path)) {
        return -1;
    }

    int inum_dir = translate(parent_path);
    if (inum_dir == -ENOENT || inum_dir == -ENOTDIR) {
        return inum_dir;
    }

    // check if file exists
    int inum = translate(path);
    if (inum > 0) {
        return -EEXIST;
    }

    struct fs_inode* parent_inode = (struct fs_inode*) malloc(sizeof(struct fs_inode));
    block_read(parent_inode, inum, 1);
    if (!S_ISDIR(parent_inode->mode)) {
        return -ENOTDIR;
    }

    int no_free_dirent = find_free_dirent_num(parent_inode);
    if(no_free_dirent < 0) {
        return -ENOSPC;
    }

    //dir check
    if (!S_ISDIR(parent_inode->mode)) {
        return -ENOTDIR;
    }
    
    // set inode region bitmap
    time_t time_raw_format;
    time( &time_raw_format );
    struct fs_inode new_inode = {
            .uid = getuid(),
            .gid = getgid(),
            .mode = mode,
            .ctime = time_raw_format,
            .mtime = time_raw_format,
            .size = 0,
            .ptrs = {0}
    };

    int free_blk_num = find_free_block_number();
    if (free_blk_num < 0) {
        return -ENOSPC;
    }
    bit_set(bitmap, free_blk_num);
    update_bitmap();

    new_inode.ptrs[0] = free_blk_num;
    int *free_block = (int *)calloc(FS_BLOCK_SIZE, sizeof(int));
    block_write(free_block, new_inode.ptrs[0], 1);

    int free_inum = find_free_inode_map_bit();
    if (free_inum < 0) {
        free(free_block);
        return -ENOSPC;
    }
    bit_set(bitmap, free_inum);
    update_bitmap();

    memcpy(&inode_region[free_inum], &new_inode, sizeof(struct fs_inode));
    update_inode(free_inum);

    // set parent_inode dirent then write it
    char *_path = strdup(path);
    char *tmp_name = get_name(_path);
    struct fs_dirent new_dirent = {
            .valid = 1,
            .inode = free_inum,
            .name = "",
    };
    assert(strlen(tmp_name) < MAX_NAME_LEN);
    memcpy(new_dirent.name, tmp_name, strlen(tmp_name));
    new_dirent.name[strlen(tmp_name)] = '\0';

    struct fs_dirent *dir = (struct fs_dirent *)malloc(MAX_DIREN_NUM);
    int blknum = (parent_inode->ptrs)[0];
    block_read(dir, blknum, 1);
    memcpy(&dir[no_free_dirent], &new_dirent, sizeof(struct fs_dirent));
    block_write(dir, blknum, 1);

    free(free_block);
    free(dir);
    free(_path);
    return 0;
}

/* unlink - delete a file
 *  success - return 0
 *  errors - path resolution, ENOENT, EISDIR
 */
int fs_unlink(const char *path) {
    /* your code here */
    int inum = translate(path);
    if (inum == -ENOENT || inum == -ENOTDIR) {
        return inum;
    }
    struct fs_inode *inode = &inode_region[inum];
    if  (S_ISDIR(inode->mode)) {
        return -EISDIR;
    }

    // YTD fs_truncate
    int truncate_result = fs_truncate(path, 0);
    if (truncate_result != 0) {
        return truncate_result;
    }

    char *parent_path;
    truncate_path(path, &parent_path);
    int parent_inum = translate(parent_path);
    free(parent_path);

    struct fs_inode* _in = (struct fs_inode*) malloc(sizeof(struct fs_inode));
    block_read(_in, parent_inum, 1);
    if (!S_ISDIR(_in->mode)) {
        return -ENOTDIR;
    }
    int blknum = _in->ptrs[0];

    // clear inode_map corresponding bit
    bit_clear(bitmap, inum);
    update_bitmap();

    // remove entry from father dir
    char *_path = strdup(path);
    char *name = get_name(_path);

    struct fs_dirent *_dir = malloc(FS_BLOCK_SIZE);
    block_read(_dir, blknum, 1);
    int found = exists_in_directory(_dir, name);
    block_write(_dir, blknum, 1);
    if (!found) {
        return -ENOENT;
    }

    free(_dir);
    free(_path);
    return 0;
}

/* rmdir - remove a directory
 *  success - return 0
 *  Errors - path resolution, ENOENT, ENOTDIR, ENOTEMPTY
 */
int fs_rmdir(const char *path) {
    /* your code here */
    int inum = translate(path);
    if (inum == -ENOENT || inum == -ENOTDIR) {
        return inum;
    }

    char *parent_path;
    int truncate_result = truncate_path(path, &parent_path);
    if (!truncate_result) {
        printf("ERROR: Deleting the root directory\n");
        return truncate_result;
    }

    int parent_inum = translate(parent_path);
    // remove entry from parent dir
    char *_path = strdup(path);
    if (_path[strlen(_path) - 1] == '/') {
		_path[strlen(_path) - 1] = '\0';
	}
    char *name = get_name(_path);
    free(parent_path);

    struct fs_inode* _in = (struct fs_inode*) malloc(sizeof(struct fs_inode));
    block_read(_in, parent_inum, 1);
    if (!S_ISDIR(_in->mode)) {
        free(_in);
        return -ENOTDIR;
    }
    int blknum = _in->ptrs[0];
    struct fs_dirent *_dir = malloc(FS_BLOCK_SIZE);
    block_read(_dir, blknum, 1);
    int found = exists_in_directory(_dir, name);

    if (!found) {
        free(_dir);
        return -ENOENT;
    }

    // clear inode_map corresponding bit
    bit_clear(bitmap, inum);
    update_bitmap();

    struct fs_dirent *_pdir = malloc(FS_BLOCK_SIZE);
    block_read(_pdir, blknum, 1);
    found = exists_in_directory(_dir, name);
    if (!found) {
        free(_pdir);
        return -ENOENT;
    }
    block_write(_dir, blknum, 1);

    free(_dir);
    free(_pdir);
    free(_path);
    free(_in);
    return 0;
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
int fs_rename(const char *src_path, const char *dst_path) {
    /* your code here */
    char *dup_src = strdup(src_path);
    char *dup_dst = strdup(dst_path);
    char *src_pathv[MAX_PATH_LEN];
    char *dst_pathv[MAX_PATH_LEN];
    int src_pathc;
    int dst_pathc;
    int same_dir = is_in_same_directory(dup_src, dup_dst, src_pathv, &src_pathc,
                                        dst_pathv, &dst_pathc);
    if (same_dir == -1) return -EINVAL;

    char *_scr_path = strdup(src_path);
    int enc_inum = find_enclosing(_scr_path);
    // enclosed directory inum.
    if (enc_inum < 0) return enc_inum;
    struct fs_inode _in;
    block_read(&_in, enc_inum, 1);

    char *src_name = src_pathv[src_pathc - 1];
    char *dst_name = dst_pathv[dst_pathc - 1];
    int blknum = _in.ptrs[0];
    struct fs_dirent direns[MAX_DIREN_NUM];
    block_read(direns, blknum, 1);
    int src_entry_idx = exists_in_directory(direns, src_name);
    // source does not exist
    if (src_entry_idx == -1) return -ENOENT;
    // destination already exists
    if (exists_in_directory(direns, dst_name) >= 0) return -EEXIST;

    memcpy(direns[src_entry_idx].name, dst_name, MAX_NAME_LEN);

    block_write(direns, blknum, 1);

    free(_scr_path);
    free(dup_src);
    free(dup_dst);
    return 0;
}

/* chmod - change file permissions
 * utime - change access and modification times
 *         (for definition of 'struct utimebuf', see 'man utime')
 *
 * success - return 0
 * Errors - path resolution, ENOENT.
 */
int fs_chmod(const char *path, mode_t mode) {
    /* your code here */
    char *_path = strdup(path);
    int inum = translate(_path);
    free(_path);
    if (inum < 0) {
        return inum;
    }
    mode_t new_permission = mode & 0000777;

    struct fs_inode _in;
    block_read(&_in, inum, 1);
    // S_IFMT     0170000   bit mask for the file type bit field
    //  chmod should only change last 9 bit of mode
    mode_t file_type = _in.mode & S_IFMT;

    _in.mode = file_type | new_permission;
    block_write(&_in, inum, 1);
    return 0;
}

int fs_utime(const char *path, struct utimbuf *ut) {
    /* your code here */
    int inum = translate(path);
    if (inum < 0) {
    	return inum;
    }
    struct fs_inode _in;
    block_read(&_in, inum, 1);

    _in.mtime = ut->modtime;
    //_in.ctime = ut->actime; not sure
    block_write(&_in, inum, 1);
    return 0;

}

/* truncate - truncate file to exactly 'len' bytes
 * success - return 0
 * Errors - path resolution, ENOENT, EISDIR, EINVAL
 *    return EINVAL if len > 0.
 */
int fs_truncate(const char *path, off_t len) {
    /* you can cheat by only implementing this for the case of len==0,
     * and an error otherwise.
     */
    if (len != 0) return -EINVAL; /* invalid argument */

    /* your code here */
    int inum = translate(path);
    if (inum == -ENOENT || inum == -ENOTDIR) {
        return inum;
    }
    struct fs_inode _in;
    block_read(&_in, inum, 1);

    if  (S_ISDIR(_in.mode)) {
        return -EISDIR;
    }

    printf("TBD\n"); // TBD update/clear block bit map & inodes

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
            struct fuse_file_info *fi) {
    /* your code here */
    int byte_read = 0;
    char *_path = strdup(path);
    int inum = translate(_path);
    if (inum == ENOENT || inum == ENOTDIR) return inum;
    struct fs_inode _in;
    block_read(&_in, inum, 1);
    // inode isn't a file return EISDIR
    if (!S_ISREG(_in.mode)) return EISDIR;

    int file_len = _in.size;
    if (offset >= file_len) return byte_read;

    int end = (offset + len >= file_len ? file_len : offset + len) - 1;
    int curr_ptr = offset;
    int buf_ptr = 0;
    for (int i = 0; curr_ptr <= end; i++) {
        // offset is smaller than current block ends, start reading.
        if (((i + 1) * FS_BLOCK_SIZE) > offset) {
            int lba = _in.ptrs[i];
            char tmp[FS_BLOCK_SIZE];
            block_read(tmp, lba, 1);

            int blck_read_start = 0;
            // offset is larger than current block start, shift blck start to
            // offset position.
            if (offset > i * FS_BLOCK_SIZE)
                blck_read_start = offset - i * FS_BLOCK_SIZE;
            int blck_ptr = blck_read_start;
            // copy until end of read or end of block.
            while (curr_ptr <= end && blck_ptr < FS_BLOCK_SIZE) {
                // printf("in fs_read: curr blck #: %d, char: %c\n buffer:
                // %s\n", i, tmp[blck_ptr], buf);
                buf[buf_ptr++] = tmp[blck_ptr++];
                curr_ptr++;
            }
            if (curr_ptr > end) break;
        }
    }
    byte_read = curr_ptr - offset;
    return byte_read;
}

/* write - write data to a file
 * success - return number of bytes written. (this will be the same as
 *           the number requested, or else it's an error)
 * Errors - path resolution, ENOENT, EISDIR
 *  return EINVAL if 'offset' is greater than current file length.
 *  (POSIX semantics support the creation of files with "holes" in them,
 *   but we don't)
 */
int fs_write(const char *path, const char *buf, size_t len, off_t offset,
             struct fuse_file_info *fi) {
    /* your code here */
    int byte_write = 0;
    char *_path = strdup(path);
    int inum = translate(_path);
    if (inum == ENOENT || inum == ENOTDIR) return inum;
    struct fs_inode _in;
    block_read(&_in, inum, 1);
    // inode isn't a file return EISDIR
    if (!S_ISREG(_in.mode)) return EISDIR;

    int file_len = _in.size;
    if (offset >= file_len) return byte_write;

    int end = (offset + len >= file_len ? file_len : offset + len) - 1;
    int curr_ptr = offset;
    int buf_ptr = 0;

    for (int i = 0; curr_ptr <= end; i++) {
        // offset is smaller than current block ends, start writing.
        if (((i + 1) * FS_BLOCK_SIZE) > offset) {
            int lba = _in.ptrs[i];
            char tmp[FS_BLOCK_SIZE];

            int blck_write_start = 0;
            // offset is larger than current block start, shift blck start to
            // offset position.
            if (offset > i * FS_BLOCK_SIZE)
                blck_write_start = offset - i * FS_BLOCK_SIZE;
            int blck_ptr = blck_write_start;
            // copy until end of read or end of block.
            while (curr_ptr <= end && blck_ptr < FS_BLOCK_SIZE) {
                // printf("in fs_write: curr blck #: %d, char: %c\n buffer:
                // %s\n", i, tmp[blck_ptr], buf);
                tmp[blck_ptr++] = buf[buf_ptr++];
                curr_ptr++;
            }
            if (curr_ptr > end) break;

            block_write(tmp, lba, 1);

        }
    }
    byte_write = curr_ptr - offset;
    return byte_write;
}

/* statfs - get file system statistics
 * see 'man 2 statfs' for description of 'struct statvfs'.
 * Errors - none. Needs to work.
 */
int fs_statfs(const char *path, struct statvfs *st) {
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
    st->f_blocks = superblock.disk_size - 2;
    int free_num = 0;
    for (int i = 0; i < superblock.disk_size; i++) {
        // TODO bit_test true if occupied?
        if (!bit_test(bitmap, i)) {
            free_num++;
        }
    }
    st->f_bfree = free_num;
    st->f_bavail = free_num;
    st->f_namemax = MAX_NAME_LEN;
    return 0;
}

/* operations vector. Please don't rename it, or else you'll break things
 */
struct fuse_operations fs_ops = {
    .init = fs_init, /* read-mostly operations */
    .getattr = fs_getattr,
    .readdir = fs_readdir,
    .rename = fs_rename,
    .chmod = fs_chmod,
    .read = fs_read,
    .statfs = fs_statfs,

    .create = fs_create, /* write operations */
    .mkdir = fs_mkdir,
    .unlink = fs_unlink,
    .rmdir = fs_rmdir,
    .utime = fs_utime,
    .truncate = fs_truncate,
    .write = fs_write,
};
