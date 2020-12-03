/*
 * file:        testing.c
 * description: libcheck test skeleton for file system project
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

// Struct definition
typedef struct {
    char *path;
    uint16_t uid;
    uint16_t gid;
    uint32_t mode;
    int32_t size;
    uint32_t ctime;
    uint32_t mtime;
} attr_t;

typedef struct {
    unsigned cksum;
    int len;
    char *path;
} cksum_t;

// Global variable

// Expected data for attributes
attr_t attr_table[] = {
    {"/", 0, 0, 040777, 4096, 1565283152, 1565283167},
    {"/file.1k", 500, 500, 0100666, 1000, 1565283152, 1565283152},
    {"/file.10", 500, 500, 0100666, 10, 1565283152, 1565283167},
    {"/dir-with-long-name", 0, 0, 040777, 4096, 1565283152, 1565283167},
    {"/dir-with-long-name/file.12k+", 0, 500, 0100666, 12289, 1565283152,
     1565283167},
    {"/dir2", 500, 500, 040777, 8192, 1565283152, 1565283167},
    {"/dir2/twenty-seven-byte-file-name", 500, 500, 0100666, 1000, 1565283152,
     1565283167},
    {"/dir2/file.4k+", 500, 500, 0100777, 4098, 1565283152, 1565283167},
    {"/dir3", 0, 500, 040777, 4096, 1565283152, 1565283167},
    {"/dir3/subdir", 0, 500, 040777, 4096, 1565283152, 1565283167},
    {"/dir3/subdir/file.4k-", 500, 500, 0100666, 4095, 1565283152, 1565283167},
    {"/dir3/subdir/file.8k-", 500, 500, 0100666, 8190, 1565283152, 1565283167},
    {"/dir3/subdir/file.12k", 500, 500, 0100666, 12288, 1565283152, 1565283167},
    {"/dir3/file.12k-", 0, 500, 0100777, 12287, 1565283152, 1565283167},
    {"/file.8k+", 500, 500, 0100666, 8195, 1565283152, 1565283167},
    {NULL}};

// Expected data for check sum
cksum_t cksum_table[] = {
    {1786485602, 1000, "/file.1k"},
    {855202508, 10, "/file.10"},
    {4101348955, 12289, "/dir-with-long-name/file.12k+"},
    {2575367502, 1000, "/dir2/twenty-seven-byte-file-name"},
    {799580753, 4098, "/dir2/file.4k+"},
    {4220582896, 4095, "/dir3/subdir/file.4k-"},
    {4090922556, 8190, "/dir3/subdir/file.8k-"},
    {3243963207, 12288, "/dir3/subdir/file.12k"},
    {2954788945, 12287, "/dir3/file.12k-"},
    {2112223143, 8195, "/file.8k+"}};

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

/* note that your tests will call:
 *  fs_ops.getattr(path, struct stat *sb)
 *  fs_ops.readdir(path, NULL, filler_function, 0, NULL)
 *  fs_ops.read(path, buf, len, offset, NULL);
 *  fs_ops.statfs(path, struct statvfs *sv);
 */

extern struct fuse_operations fs_ops;
extern void block_init(char *file);

/**
 * @brief Sample test for get_attr
 *
 */
START_TEST(getattr_sample_test) {
    struct stat *sb = malloc(sizeof(*sb));
    fs_ops.getattr("/", sb);
    printf(
        "Actual:uid: %u \t gid: %u \t mode: %u \n size: %lu "
        "\t ctime: %lu \t mtime: %lu\n",
        sb->st_uid, sb->st_gid, sb->st_mode, sb->st_size, sb->st_ctim.tv_sec,
        sb->st_mtim.tv_sec);
    ck_assert_int_eq(sb->st_gid, 0);
}
END_TEST

/**
 * @brief Test for get_attr
 *
 */
START_TEST(getattr_test) {
    int i = 0;
    for (i = 0; attr_table[i].path != NULL; i++) {
        printf(
            "Path is %s\n Expected: uid: %u \t gid: %u \t mode: %u \n size: %u "
            "\t ctime: %u \t mtime: %u\n",
            attr_table[i].path, attr_table[i].uid, attr_table[i].gid,
            attr_table[i].mode, attr_table[i].size, attr_table[i].ctime,
            attr_table[i].mtime);

        struct stat *sb = malloc(sizeof(*sb));
        fs_ops.getattr(attr_table[i].path, sb);
        printf(
            "Actual:uid: %u \t gid: %u \t mode: %u \n size: %lu "
            "\t ctime: %lu \t mtime: %lu\n",
            sb->st_uid, sb->st_gid, sb->st_mode, sb->st_size,
            sb->st_ctim.tv_sec, sb->st_mtim.tv_sec);

        ck_assert_int_eq(attr_table[i].uid, sb->st_uid);
        ck_assert_int_eq(attr_table[i].gid, sb->st_gid);
        ck_assert_int_eq(attr_table[i].mode, sb->st_mode);
        ck_assert_int_eq(attr_table[i].size, sb->st_size);

        // As per stat.h doc, defined st_ctime as st_ctim.tv_sec for Backward
        // Compatibility, use tv_sec for integer comparison.
        ck_assert_int_eq(attr_table[i].ctime, sb->st_ctim.tv_sec);
        ck_assert_int_eq(attr_table[i].mtime, sb->st_mtim.tv_sec);
    }
}
END_TEST

/**
 * @brief Test for fs_read - single big read
 * int fs_read(const char *path, char *buf, size_t len, off_t offset,
            struct fuse_file_info *fi)
 */
START_TEST(single_read_test) {
    int i = 0;
    for (i = 0; cksum_table[i].path != NULL; i++) {
        printf(
            "Path is %s\t read whole file \n Expected: checksum "
            "is %u\n",
            cksum_table[i].path, cksum_table[i].cksum);
        // TODO: malloc the buff  ?
        char *buf = NULL;
        fs_ops.read(cksum_table[i].path, buf, cksum_table[i].len, 0, NULL);
        // TODO: crc32 buf type mismatch
        // Cast to unsigned char avoid warning
        unsigned cksum = crc32(0, (unsigned char *)buf, cksum_table[i].len);
        ck_assert_int_eq(cksum_table[i].cksum, cksum);
    }
}
END_TEST

int main(int argc, char **argv) {
    block_init("test.img");
    fs_ops.init(NULL);

    Suite *s = suite_create("fs5600");
    TCase *tc = tcase_create("read_mostly");
    TCase *tc_sample_getattr = tcase_create("get_attr sample test");
    TCase *tc_getattr = tcase_create("get_attr test");

    TCase *tc_single_read = tcase_create("single read test");

    tcase_add_test(tc, a_test); /* see START_TEST above */
    /* add more tests here */
    tcase_add_test(tc_sample_getattr, getattr_sample_test);
    tcase_add_test(tc_getattr, getattr_test);

    tcase_add_test(tc_single_read, single_read_test);

    suite_add_tcase(s, tc);
    /* TODO: Uncomment below testcases one by one.*/

    suite_add_tcase(s, tc_sample_getattr);
    suite_add_tcase(s, tc_getattr);

    // suite_add_tcase(s, tc_single_read);

    SRunner *sr = srunner_create(s);
    srunner_set_fork_status(sr, CK_NOFORK);

    srunner_run_all(sr, CK_VERBOSE);
    int n_failed = srunner_ntests_failed(sr);
    printf("%d tests failed\n", n_failed);

    srunner_free(sr);
    return (n_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
