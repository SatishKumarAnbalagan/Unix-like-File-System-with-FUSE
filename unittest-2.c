/*
 * file:        unittest-2.c
 * description: libcheck test skeleton, part 2
 */

#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION 26
#define FS_BLOCK_SIZE 4096

#include <check.h>
#include <errno.h>
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

/* mockup for fuse_get_context. you can change ctx.uid, ctx.gid in
 * tests if you want to test setting UIDs in mknod/mkdir
 */
struct fuse_context ctx = {.uid = 500, .gid = 500};
struct fuse_context *fuse_get_context(void) {
    return &ctx;
}
/* note that your tests will call:
 *  fs_ops.getattr(path, struct stat *sb)
 *  fs_ops.readdir(path, NULL, filler_function, 0, NULL)
 *  fs_ops.read(path, buf, len, offset, NULL);
 *  fs_ops.statfs(path, struct statvfs *sv);
 */

extern struct fuse_operations fs_ops;
extern void block_init(char *file);

typedef struct {
    char *path;
    uint16_t uid;
    uint16_t gid;
    uint32_t mode;
    int32_t size;
    uint32_t ctime;
    uint32_t mtime;
    uint16_t blck_num;
} attr_t;

typedef struct {
    char *childpath;
    int found;
} readdirtest_t;

attr_t attr_table[] = {
    {"/", 0, 0, 040777, 4096, 1565283152, 1565283167, 1},
    {"/file.1k", 500, 500, 0100666, 1000, 1565283152, 1565283152, 1},
    {"/file.10", 500, 500, 0100666, 10, 1565283152, 1565283167, 1},
    {"/dir-with-long-name", 0, 0, 040777, 4096, 1565283152, 1565283167, 1},
    {"/dir-with-long-name/file.12k+", 0, 500, 0100666, 12289, 1565283152,
     1565283167, 4},
    {"/dir2", 500, 500, 040777, 8192, 1565283152, 1565283167, 2},
    {"/dir2/twenty-seven-byte-file-name", 500, 500, 0100666, 1000, 1565283152,
     1565283167, 1},
    {"/dir2/file.4k+", 500, 500, 0100777, 4098, 1565283152, 1565283167, 2},
    {"/dir3", 0, 500, 040777, 4096, 1565283152, 1565283167, 1},
    {"/dir3/subdir", 0, 500, 040777, 4096, 1565283152, 1565283167, 1},
    {"/dir3/subdir/file.4k-", 500, 500, 0100666, 4095, 1565283152, 1565283167,
     1},
    {"/dir3/subdir/file.8k-", 500, 500, 0100666, 8190, 1565283152, 1565283167,
     2},
    {"/dir3/subdir/file.12k", 500, 500, 0100666, 12288, 1565283152, 1565283167,
     3},
    {"/dir3/file.12k-", 0, 500, 0100777, 12287, 1565283152, 1565283167, 3},
    {"/file.8k+", 500, 500, 0100666, 8195, 1565283152, 1565283167, 3},
    {NULL}};

/* this is an example of a callback function for readdir
 */
int empty_filler(void *ptr, const char *name, const struct stat *stbuf,
                 off_t off) {
    /* FUSE passes you the entry name and a pointer to a 'struct stat'
     * with the attributes. Ignore the 'ptr' and 'off' arguments
     *
     */
    return 0;
}

int test_filler(void *ptr, const char *name, const struct stat *st, off_t off) {
    // direntry_t *table = ptr;
    readdirtest_t *test = ptr;

    printf("file: %s, mode: %o, blck_num: %ld\n", name, st->st_mode,
           st->st_blocks);
    if (strcmp(test->childpath, name) == 0) {
        test->found = 1;
        printf("\nchild found name: %s\n", name);
    }
    return 1;
}

START_TEST(fs_mkdir_single_test) {
    printf("fs_mkdir_single_test\n\n");
    const char *parentdir = "/dir2";
    char *newdir = "/dir2/newdir";
    mode_t mode = 0777;
    int mkdir_status = fs_ops.mkdir(newdir, mode);
    printf("mkdir: %s, status is %d\n", newdir, mkdir_status);
    ck_assert_int_eq(mkdir_status, 0);

    printf("\n Parent dir: %s\n", parentdir);
    // int fs_readdir(const char *path, void *ptr, fuse_fill_dir_t filler,
    //                off_t offset, struct fuse_file_info *fi)
    readdirtest_t readdirtest = {newdir, 0};

    int read_status =
        fs_ops.readdir(parentdir, &readdirtest, test_filler, 0, NULL);
    ck_assert_int_eq(read_status, 0);
    if (readdirtest.found == 0) {
        printf("new directory not found. Mkdir error\n");
        ck_abort();
    }
}
END_TEST

readdirtest_t mkdir_table[] = {{"mkdir1", 0},
                               {"mkdir2", 0},
                               {"mkdir3", 0},
                               {"mkdir4", 0},
                               {NULL}};

START_TEST(fs_mkdir_test) {
    printf("fs_mkdir_test\n\n");

    const char *parentdir = "/dir3/";
    mode_t mode = 0777;
    printf("\n Parent dir: %s\n", parentdir);
    for (int i = 0; mkdir_table[i].childpath != NULL; i++) {

        char combined_path[100];
        sprintf(combined_path, "%s%s", parentdir, mkdir_table[i].childpath);
        int mkdir_status = fs_ops.mkdir(combined_path, mode);
        printf("mkdir: %s, status is %d\n", mkdir_table[i].childpath,
               mkdir_status);
        ck_assert_int_eq(mkdir_status, 0);
    }

    for (int j = 0; mkdir_table[j].childpath != NULL; j++) {
        // int fs_readdir(const char *path, void *ptr, fuse_fill_dir_t filler,
        //                off_t offset, struct fuse_file_info *fi)
        readdirtest_t readdirtest = {mkdir_table[j].childpath, 0};

        int read_status =
            fs_ops.readdir(parentdir, &readdirtest, test_filler, 0, NULL);
        ck_assert_int_eq(read_status, 0);
        if (readdirtest.found == 0) {
            printf("new directory not found. Mkdir error\n");
            ck_abort();
        }
    }
}
END_TEST

// make empty dir first and then remove it.
START_TEST(fs_rmdir_test) {
    printf("fs_rmdir_test\n\n");

    // check empty dir created.
    const char *parentdir = "/dir2";
    char *newdir = "/dir2/newdir";
    mode_t mode = 0777;
    int mkdir_status = fs_ops.mkdir(newdir, mode);
    printf("mkdir: %s, status is %d\n", newdir, mkdir_status);
    ck_assert_int_eq(mkdir_status, 0);

    printf("\n Parent dir: %s\n", parentdir);
    // int fs_readdir(const char *path, void *ptr, fuse_fill_dir_t filler,
    //                off_t offset, struct fuse_file_info *fi)
    readdirtest_t mkdir_read = {newdir, 0};

    int read_status =
        fs_ops.readdir(parentdir, &mkdir_read, test_filler, 0, NULL);
    ck_assert_int_eq(read_status, 0);
    if (mkdir_read.found == 0) {
        printf("new directory not found. Mkdir error\n");
        ck_abort();
    }

    // check if empty dir removed.
    int rm_status = fs_ops.rmdir(newdir);
    printf("rmdir: %s, status is %d", newdir, rm_status);
    readdirtest_t rmdir_read = {newdir, 0};

    read_status = fs_ops.readdir(parentdir, &rmdir_read, test_filler, 0, NULL);
    ck_assert_int_eq(read_status, 0);
    if (rmdir_read.found == 1) {
        printf("new directory found. Rmdir error.\n");
        ck_abort();
    }
}
END_TEST

readdirtest_t fscreate_table[] = {{"create1", 0},
                                  {"create2", 0},
                                  {"create3", 0},
                                  {"create4", 0},
                                  {NULL}};

void fscreate_test() {
    const char *parentdir = "/dir3/";
    mode_t mode = 0777;
    printf("\n Parent dir: %s\n", parentdir);
    for (int i = 0; fscreate_table[i].childpath != NULL; i++) {
        char combined_path[100];
        sprintf(combined_path, "%s%s", parentdir, fscreate_table[i].childpath);
        int mkdir_status =
            fs_ops.create(combined_path, mode, NULL);
        printf("create node: %s, status is %d\n", fscreate_table[i].childpath,
               mkdir_status);
        ck_assert_int_eq(mkdir_status, 0);
    }

    for (int j = 0; fscreate_table[j].childpath != NULL; j++) {
        // int fs_readdir(const char *path, void *ptr, fuse_fill_dir_t filler,
        //                off_t offset, struct fuse_file_info *fi)

        int read_status =
            fs_ops.readdir(parentdir, &fscreate_table[j], test_filler, 0, NULL);
        ck_assert_int_eq(read_status, 0);
        if (fscreate_table[j].found == 0) {
            printf("new file: %s not found. create error\n", fscreate_table[j].childpath);
            ck_abort();
        }
        // reset table.
        fscreate_table[j].found = 0;
    }
}

START_TEST(fs_create_test) {
    printf("fscreate_test\n\n");
    fscreate_test();
}
END_TEST

START_TEST(fs_unlink_test) {
    printf("fs_unlink_test\n\n");

    fscreate_test();
    const char *parentdir = "/dir3";
    printf("\n Parent dir: %s\n", parentdir);
    for (int i = 0; fscreate_table[i].childpath != NULL; i++) {
        int mkdir_status = fs_ops.unlink(fscreate_table[i].childpath);
        printf("remove node: %s, status is %d\n", fscreate_table[i].childpath,
               mkdir_status);
        ck_assert_int_eq(mkdir_status, 0);
    }

    for (int j = 0; fscreate_table[j].childpath != NULL; j++) {
        // int fs_readdir(const char *path, void *ptr, fuse_fill_dir_t filler,
        //                off_t offset, struct fuse_file_info *fi)

        int read_status =
            fs_ops.readdir(parentdir, &fscreate_table[j], test_filler, 0, NULL);
        ck_assert_int_eq(read_status, 0);
        if (fscreate_table[j].found != 0) {
            printf("new directory found. unlink error\n");
            ck_abort();
        }
    }
}
END_TEST

START_TEST(fsmknod_error_test) {
    printf("fsmknod_error_test\n");

    // bad path /a/b/c - b doesn't exist (should return -ENOENT)
    mode_t mode = 0777;
    int expected = -ENOENT;
    printf("bad path /a/b/c - b doesn't exist (should return %d)\n", expected);
    int actual = fs_ops.create("/dir3/notexist/file.4k-", mode, NULL);
    printf("actual status: %d\n", actual);
    ck_assert_int_eq(expected, actual);

    // bad path /a/b/c - b isn't directory (ENOTDIR)
    expected = -ENOTDIR;
    printf("bad path /a/b/c - b isn't directory (should return %d)\n",
           expected);
    actual = fs_ops.create("/dir3/file.12k-/file.4k-", mode, NULL);
    printf("actual status: %d\n", actual);
    ck_assert_int_eq(expected, actual);

    // bad path /a/b/c - c exists, is file (EEXIST)
    expected = -EEXIST;
    printf(" bad path /a/b/c - c exists, is file (should return %d)\n",
           expected);
    actual = fs_ops.create("/dir3/subdir/file.4k-", mode, NULL);
    printf("actual status: %d\n", actual);
    ck_assert_int_eq(expected, actual);

    // bad path /a/b/c - c exists, is directory (EEXIST)
    expected = -EEXIST;
    printf("bad path /a/b/c - c exists, is directory(should return %d)\n",
           expected);
    actual = fs_ops.create("/dir3/subdir", mode, NULL);
    printf("actual status: %d\n", actual);
    ck_assert_int_eq(expected, actual);

    // too-long name (more than 27 characters)  either return -EINVAL or
    // truncate the name.

    expected = 0;
    printf(
        "too-long name (more than 27 characters) (should return %d or truncate "
        "the name)\n",
        expected);
    actual = fs_ops.create(
        "/dir3/"
        "longnamefilelongnamefilelongnamefilelongnamefilelongnamefile.txt",
        mode, NULL);
    printf("actual status: %d\n", actual);
    ck_assert_int_eq(expected, actual);

    // TODO: test truncate name
}
END_TEST

START_TEST(fsmkdir_error_test) {
    printf("fsmkdir_error_test\n");
    //     bad path /a/b/c - b doesn't exist (ENOENT)
    mode_t mode = 0777;
    int expected = -ENOENT;
    printf("bad path /a/b/c - b doesn't exist (should return %d)\n", expected);
    int actual = fs_ops.mkdir("/dir3/notexist/newdir", mode);
    printf("actual status: %d\n", actual);
    ck_assert_int_eq(expected, actual);

    // bad path /a/b/c - b isn't directory (ENOTDIR)
    expected = -ENOTDIR;
    printf("bad path /a/b/c - b isn't directory (should return %d)\n",
           expected);
    actual = fs_ops.mkdir("/dir3/file.12k-/newdir", mode);
    printf("actual status: %d\n", actual);
    ck_assert_int_eq(expected, actual);

    // bad path /a/b/c - c exists, is file (EEXIST)
    expected = -EEXIST;
    printf(" bad path /a/b/c - c exists, is file (should return %d)\n",
           expected);
    actual = fs_ops.mkdir("/dir3/subdir/file.4k-", mode);
    printf("actual status: %d\n", actual);
    ck_assert_int_eq(expected, actual);

    // bad path /a/b/c - c exists, is directory (EEXIST)
    expected = -EEXIST;
    printf("bad path /a/b/c - c exists, is directory(should return %d)\n",
           expected);
    actual = fs_ops.mkdir("/dir3/subdir", mode);
    printf("actual status: %d\n", actual);
    ck_assert_int_eq(expected, actual);

    // too-long name (more than 27 characters)  either return -EINVAL or
    // truncate the name.

    expected = -EINVAL;
    printf(
        "too-long name (more than 27 characters) (should return %d or truncate "
        "the name)\n",
        expected);
    actual = fs_ops.mkdir(
        "/dir3/"
        "longnamedirlongnamedirlongnamedirlongnamedirlongnamedirlongnamedir",
        mode);
    printf("actual status: %d\n", actual);
    ck_assert_int_eq(expected, actual);
}
END_TEST

START_TEST(fs_unlink_error_test) {
    printf("fs_unlink_error_test\n");
    //     bad path /a/b/c - b doesn't exist (ENOENT)
    int expected = -ENOENT;
    printf("bad path /a/b/c - b doesn't exist (should return %d)\n", expected);
    int actual = fs_ops.unlink("/dir3/notexist/file.4k-");
    printf("actual status: %d\n", actual);
    ck_assert_int_eq(expected, actual);

    // bad path /a/b/c - b isn't directory (ENOTDIR)
    expected = -ENOTDIR;
    printf("bad path /a/b/c - b isn't directory (should return %d)\n",
           expected);
    actual = fs_ops.unlink("/dir3/file.12k-/file.4k-");
    printf("actual status: %d\n", actual);
    ck_assert_int_eq(expected, actual);

    // bad path /a/b/c - c not exists, is file (ENOENT)
    expected = -ENOENT;
    printf(" bad path /a/b/c - c exists, is file (should return %d)\n",
           expected);
    actual = fs_ops.unlink("/dir3/subdir/notexist.txt");
    printf("actual status: %d\n", actual);
    ck_assert_int_eq(expected, actual);

    // bad path /a/b/c - c exists, is directory (EISDIR)
    expected = -EISDIR;
    printf("bad path /a/b/c - c exists, is directory(should return %d)\n",
           expected);
    actual = fs_ops.unlink("/dir3/subdir");
    printf("actual status: %d\n", actual);
    ck_assert_int_eq(expected, actual);
}
END_TEST

START_TEST(fs_rmdir_error_test) {
    printf("fs_rmdir_error_test\n");
    //     bad path /a/b/c - b doesn't exist (ENOENT)
    int expected = -ENOENT;
    printf("bad path /a/b/c - b doesn't exist (should return %d)\n", expected);
    int actual = fs_ops.rmdir("/notexist/subdir");
    printf("actual status: %d\n", actual);
    ck_assert_int_eq(expected, actual);

    // bad path /a/b/c - b isn't directory (ENOTDIR)
    expected = -ENOTDIR;
    printf("bad path /a/b/c - b isn't directory (should return %d)\n",
           expected);
    actual = fs_ops.rmdir("/dir3/file.12k-/shouldGenerateErorBefore");
    printf("actual status: %d\n", actual);
    ck_assert_int_eq(expected, actual);

    // bad path /a/b/c - c doesn't exist (ENOENT)
    expected = -ENOENT;
    printf(" bad path /a/b/c - c doesn't exist (should return %d)\n", expected);
    actual = fs_ops.unlink("/dir3/subdir/notexist");
    printf("actual status: %d\n", actual);
    ck_assert_int_eq(expected, actual);

    // bad path /a/b/c - c is file (ENOTDIR)
    expected = -EISDIR;
    char *enotdirpath = "/dir3/file.12k-";
    printf("bad path: %s \t /a/b/c - c exists, is file(should return %d)\n",
           enotdirpath, expected);
    actual = fs_ops.unlink(enotdirpath);
    printf("actual status: %d\n", actual);
    ck_assert_int_eq(expected, actual);

    // directory not empty (ENOTEMPTY)
    expected = -ENOTEMPTY;
    char *ENOTEMPTY_path = "/dir3/subdir";
    printf("bad path: %s \t directory not empty (should return %d)\n",
           ENOTEMPTY_path, expected);
    actual = fs_ops.unlink(ENOTEMPTY_path);
    printf("actual status: %d\n", actual);
    ck_assert_int_eq(expected, actual);
}
END_TEST

void gen_buff(char *buf, int len) {
    char *ptr = buf;
    int i;
    for (i = 0, ptr = buf; ptr < buf + len; i++) {
        ptr += sprintf(ptr, "%d ", i);
    }
}

void verify_write(char *path, int len, int offset, unsigned expect_cksum) {
    char *read_buf = malloc(sizeof(char) * len);
    int byte_read = fs_ops.read(path, read_buf, len, offset, NULL);
    printf(
        "Test by reading, offset: %d, Expected read len: %d, Actual byte read "
        "len: %d\n",
        offset, len, byte_read);

    ck_assert_int_eq(len, byte_read);
    unsigned read_cksum = crc32(0, (unsigned char *)read_buf, len);
    printf("Expected cksum %u \t Read checksum is %u \n", expect_cksum,
           read_cksum);

    ck_assert_int_eq(expect_cksum, read_cksum);
    free(read_buf);
}

/* change test name and make it do something useful */
START_TEST(write_smallfile_test) {
    char path[] = "/file.10";
    int len = 4000;
    char *write_buf = malloc(len);
    gen_buff(write_buf, len);
    unsigned write_cksum = crc32(0, (unsigned char *)write_buf, len);
    printf("Path to write: %s \t size to write: %d \t expected cksum: %u \n",
           path, len, write_cksum);
    int byte_written = fs_ops.write(path, write_buf, len, 0,
                                    NULL);  // 4000 bytes, offset=0
    printf("Byte writte: %d\n", byte_written);

    ck_assert_int_eq(byte_written, len);

    // Test by reading the written file.
    verify_write(path, len, 0, write_cksum);
    free(write_buf);
}
END_TEST

START_TEST(fswrite_test) {
    char *path[] = {"/file.1k", NULL};
    int lens[] = {4000};
    for (int i = 0; path[i] != NULL; i++) {
        int len = lens[i];
        char *write_buf = malloc(len);
        gen_buff(write_buf, len);
        unsigned write_cksum = crc32(0, (unsigned char *)write_buf, len);
        printf("Path to write: %s \t size to write: %d \t cksum: %u \n",
               path[i], len, write_cksum);
        int byte_written = fs_ops.write(path[i], write_buf, len, 0,
                                        NULL);  // 4000 bytes, offset=0
        printf("Byte writte: %d\n", byte_written);

        ck_assert_int_eq(byte_written, len);

        // Test by reading the written file.
        verify_write(path[i], len, 0, write_cksum);
        free(write_buf);
    }
}
END_TEST

// int fs_write(const char *path, const char *buf, size_t len, off_t offset,
//              struct fuse_file_info *fi)
START_TEST(fswrite_append_test) {
    char *paths[] = {"/OneBlock", NULL};
    int init_lens[] = {FS_BLOCK_SIZE};
    int after_append_lens[] = {FS_BLOCK_SIZE * 2 - 1};
    int steps[] = {17, 100, 1000, 1024, 1970, 3000};

    for (int i = 0; paths[i] != NULL; i++) {
        // TODO: may be wrong here
        for (int j = 0; j < 6; j++) {
            char m_path[40];
            sprintf(m_path, "%s.%d", paths[i], steps[j]);
            int file_len = after_append_lens[i];

            char *buf = malloc(file_len);
            gen_buff(buf, init_lens[i]);

            int byte_written = fs_ops.write(paths[i], buf, init_lens[i], 0,
                                            NULL);  // 4000 bytes, offset=0
            printf("Init Byte writte: %d ", byte_written);
            printf("Path is %s\t small chunk append step is %d\n", m_path,
                   steps[j]);

            // append to file.
            for (int offset = init_lens[i]; offset < file_len;
                 offset += steps[j]) {
                int len_to_write = steps[j];
                if (steps[j] + offset > file_len) {
                    len_to_write = file_len - offset;
                }
                gen_buff(buf + offset, len_to_write);
                fs_ops.write(m_path, buf + offset, len_to_write, offset, NULL);
            }

            unsigned expect_cksum = crc32(0, (unsigned char *)buf, file_len);

            verify_write(m_path, file_len, 0, expect_cksum);

            free(buf);
        }
    }
}
END_TEST

void reset_testdata() {
    for (int i = 0; mkdir_table[i].childpath != NULL; i++) {
        mkdir_table[i].found = 0;
    }
    for (int i = 0; fscreate_table[i].childpath != NULL; i++) {
        fscreate_table[i].found = 0;
    }
}

void setup() {
    printf(">>> ");
    // Regenerate the test image each time of tests.
    system("python gen-disk.py -q disk1.in test.img");
    reset_testdata();
}

void setupTestcase(Suite *s, const char *str, void (*f)(int)) {
    TCase *tc = tcase_create(str);
    tcase_add_test(tc, f);
    tcase_add_unchecked_fixture(tc, setup, NULL);
    suite_add_tcase(s, tc);
}

int main(int argc, char **argv) {
    // Use part 1 test image instead of empty image
    block_init("test.img");
    fs_ops.init(NULL);

    Suite *s = suite_create("fs5600");

    /* add more tests here */

    SRunner *sr = srunner_create(s);

    setupTestcase(s, "fs_mkdir_test", fs_mkdir_test);
    // setupTestcase(s, "rmdir single test", fs_rmdir_test);
    // setupTestcase(s, "create test", fs_create_test);
    // setupTestcase(s, "fs_unlink_test", fs_unlink_test);
    // setupTestcase(s, "fs_mkdir_single_test", fs_mkdir_single_test);

    // setupTestcase(s, "fs_rmdir_error_test", fs_rmdir_error_test);
    // setupTestcase(s, "fs_unlink_error_test", fs_unlink_error_test);
    // setupTestcase(s, "fsmkdir_error_test", fsmkdir_error_test);
    // setupTestcase(s, "fsmknod_error_test", fsmknod_error_test);
    // setupTestcase(s, "fswrite_append_test", fswrite_append_test);
    // setupTestcase(s, "fswrite_test", fswrite_test);
    // setupTestcase(s, "write_smallfile_test", write_smallfile_test);
    srunner_set_fork_status(sr, CK_NOFORK);

    srunner_run_all(sr, CK_VERBOSE);
    int n_failed = srunner_ntests_failed(sr);
    printf("%d tests failed\n", n_failed);

    srunner_free(sr);
    return (n_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
