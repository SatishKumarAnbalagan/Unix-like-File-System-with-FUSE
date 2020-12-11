/*
 * file:        unittest-2.c
 * description: libcheck test skeleton, part 2
 */

#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION 26

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

/* change test name and make it do something useful */
START_TEST(a_test) { ck_assert_int_eq(1, 1); }
END_TEST

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
        printf("child found name: %s", name);
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

readdirtest_t mkdir_table[] = {{"/dir3/mkdir1", 0},
                               {"/dir3/mkdir2", 0},
                               {"/dir3/mkdir3", 0},
                               {"/dir3/mkdir4", 0},
                               {NULL}};

START_TEST(fs_mkdir_test) {
    printf("fs_mkdir_test\n\n");

    const char *parentdir = "/dir3";
    mode_t mode = 0777;
    printf("\n Parent dir: %s\n", parentdir);
    for (int i = 0; mkdir_table[i].childpath != NULL; i++) {
        int mkdir_status = fs_ops.mkdir(mkdir_table[i].childpath, mode);
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

readdirtest_t fscreate_table[] = {{"/dir3/create1", 0},
                                  {"/dir3/create2", 0},
                                  {"/dir3/create3", 0},
                                  {"/dir3/create4", 0},
                                  {NULL}};

void fscreate_test() {
    const char *parentdir = "/dir3";
    mode_t mode = 0777;
    printf("\n Parent dir: %s\n", parentdir);
    for (int i = 0; fscreate_table[i].childpath != NULL; i++) {
        int mkdir_status =
            fs_ops.create(fscreate_table[i].childpath, mode, NULL);
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
            printf("new directory not found. create error\n");
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
    block_init("test2.img");
    fs_ops.init(NULL);

    Suite *s = suite_create("fs5600");
    TCase *tc = tcase_create("write_mostly");

    tcase_add_test(tc, a_test); /* see START_TEST above */
    /* add more tests here */

    suite_add_tcase(s, tc);
    SRunner *sr = srunner_create(s);

    setupTestcase(s, "fs_mkdir_test", fs_mkdir_test);
    setupTestcase(s, "rmdir single test", fs_rmdir_test);
    setupTestcase(s, "create test", fs_create_test);
    setupTestcase(s, "fs_unlink_test", fs_unlink_test);
    setupTestcase(s, "fs_mkdir_single_test", fs_mkdir_single_test);

    srunner_set_fork_status(sr, CK_NOFORK);

    srunner_run_all(sr, CK_VERBOSE);
    int n_failed = srunner_ntests_failed(sr);
    printf("%d tests failed\n", n_failed);

    srunner_free(sr);
    return (n_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
