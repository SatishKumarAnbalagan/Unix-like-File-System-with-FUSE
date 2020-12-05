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

typedef struct {
    char *name;
    int seen;
} direntry_t;

direntry_t root_table[] = {
    {"dir2", 0},    {"dir3", 0},    {"dir-with-long-name", 0},
    {"file.10", 0}, {"file.1k", 0}, {"file.8k+"},
    {NULL}};
direntry_t dir2_table[] = {
    {"twenty-seven-byte-file-name", 0}, {"file.4k+", 0}, {NULL}};
direntry_t dir3_table[] = {{"subdir", 0}, {"file.12k-", 0}, {NULL}};
direntry_t dir3sub_table[] = {
    {"file.4k-", 0}, {"file.8k-", 0}, {"file.12k", 0}, {NULL}};
direntry_t dirlongname_table[] = {{"file.12k+", 0}, {NULL}};

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
    free(sb);
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
        free(sb);
    }
}
END_TEST

START_TEST(getattr_error_test) {
    int count = 4;
    char *paths[] = {"/not-a-file", "/file.1k/file.0", "/not-a-dir/file.0",
                     "/dir2/not-a-file"};
    int expected_error[] = {ENOENT, ENOTDIR, ENOENT, ENOENT};
    for (int i = 0; i < count; i++) {
        printf("path is %s, expect error is %d \n", paths[i],
               expected_error[i]);
        struct stat *sb = malloc(sizeof(*sb));
        int error = fs_ops.getattr(paths[i], sb);
        printf("Actual error: %d \n", error);
        ck_assert_int_eq(expected_error[i], -error);
        free(sb);
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
            "\nPath is %s\t read whole file \n Expected: checksum "
            "is %u\n",
            cksum_table[i].path, cksum_table[i].cksum);
        char *buf = malloc(sizeof(char) * cksum_table[i].len);
        fs_ops.read(cksum_table[i].path, buf, cksum_table[i].len, 0, NULL);
        // TODO: crc32 buf type mismatch
        // Cast to unsigned char avoid warning
        // if (i == 1) {
        //     printf("read file content: %s\n", buf);
        // }
        unsigned cksum = crc32(0, (unsigned char *)buf, cksum_table[i].len);
        printf(" Actual: checksum is %u \n", cksum);
        ck_assert_int_eq(cksum_table[i].cksum, cksum);
        free(buf);
    }
}
END_TEST

int test_filler(void *ptr, const char *name, const struct stat *st, off_t off) {
    direntry_t *table = ptr;

    printf("file: %s, mode: %o\n", name, st->st_mode);
    for (int i = 0; table[i].name != NULL; i++) {
        if (strcmp(table[i].name, name) == 0) {
            table[i].seen = 1;

            return 0;
        }
    }
    printf("actual directory entry: %s not found in expected.", name);
    ck_abort();
    return 1;
}

void readdir_test_helper(direntry_t *table, char *path) {
    printf("Expected entries:");
    int i = 0;
    for (i = 0; table[i].name != NULL; i++) {
        printf(" %s", table[i].name);
    }
    printf("\nActual entries: ");
    // int fs_readdir(const char *path, void *ptr, fuse_fill_dir_t filler,
    //                off_t offset, struct fuse_file_info *fi)
    int status = fs_ops.readdir(path, table, test_filler, 0, NULL);
    printf("status is %d", status);
    ck_assert_int_eq(status, 0);
    for (i = 0; table[i].name != NULL; i++) {
        if (!table[i].seen) {
            printf("entry: %s isn't seen in readdir: %s\n ", table[i].name,
                   path);
            ck_abort();
        }
        // reset seen status.
        table[i].seen = 0;
    }
}

START_TEST(readdir_test) {
    char *paths[] = {"/", "/dir2", "/dir3", "/dir3/subdir",
                     "/dir-with-long-name"};
    direntry_t *tables[] = {root_table, dir2_table, dir3_table, dir3sub_table,
                            dirlongname_table};
    for (int i = 0; i < 5; i++) {
        printf("\nreaddir path: %s\n", paths[i]);
        readdir_test_helper(tables[i], paths[i]);
    }
}
END_TEST

START_TEST(readdir_error_test) {
    char *paths[] = {"/dir2/file.4k+", "/dir2/notexist"};
    int expected_error[] = {ENOTDIR, ENOENT};
    for (int i = 0; i < 2; i++) {
        printf("readdir path: %s \n", paths[i]);
        int status = fs_ops.readdir(paths[i], dir2_table, test_filler, 0, NULL);
        printf("Expected error: %d \t", expected_error[i]);
        printf("Actual error: %d\n", -status);
        ck_assert_int_eq(-status, expected_error[i]);
    }
}
END_TEST

START_TEST(fsread_small_read_test) {
    int i = 0;
    int steps[] = {17, 100, 1000, 1024, 1970, 3000};
    for (i = 0; cksum_table[i].path != NULL; i++) {
        printf(
            "\nPath is %s\t small chunk reads \n Expected: checksum "
            "is %u\n",
            cksum_table[i].path, cksum_table[i].cksum);

        for (int j = 0; j < 6; j++) {
            int file_len = cksum_table[i].len;
            char *buf = malloc(sizeof(char) * cksum_table[i].len);
            for (int offset = 0; offset < file_len; offset += steps[j]) {
                fs_ops.read(cksum_table[i].path, buf + offset, steps[j], offset,
                            NULL);
            }
            // TODO: crc32 buf type mismatch
            // Cast to unsigned char avoid warning
            unsigned cksum = crc32(0, (unsigned char *)buf, cksum_table[i].len);
            printf(" Actual: checksum is %u for chuck size: %d\n", cksum,
                   steps[j]);
            ck_assert_int_eq(cksum_table[i].cksum, cksum);
            free(buf);
        }
    }
}
END_TEST

// START_TEST(fsread_chunck_sample_test) {
//     int i = 0;
//     printf(
//         "\nPath is %s\t small chunk reads \n Expected: checksum "
//         "is %u\n",
//         cksum_table[i].path, cksum_table[i].cksum);

//     int file_len = cksum_table[i].len;
//     char *buf = malloc(sizeof(char) * cksum_table[i].len);
//     int step = 17;
//     for (int offset = 0; offset < file_len; offset += step) {
//         printf("read param: step: %d, offset: %d \n", step, offset);
//         fs_ops.read(cksum_table[i].path, buf + offset, step, offset, NULL);
//     }
//     // TODO: crc32 buf type mismatch
//     // Cast to unsigned char avoid warning
//     unsigned cksum = crc32(0, (unsigned char *)buf, cksum_table[i].len);
//     printf(" Actual: checksum is %u for chuck size: %d\n", cksum, step);
//     printf(" Read result: %s\n", buf);
//     ck_assert_int_eq(cksum_table[i].cksum, cksum);
// }
// END_TEST

START_TEST(fsstat_test) {
    struct statvfs expected;
    expected.f_bsize = 4096;
    expected.f_blocks = 398;
    expected.f_bfree = 355;
    expected.f_bavail = 355;
    expected.f_namemax = 27;
    printf("File system stats test\n ");
    printf(
        "Expected: bsize: %ld, blocks: %ld, free blocks: %ld\n available "
        "blocks: "
        "%ld, max name length: %ld\n",
        expected.f_bsize, expected.f_blocks, expected.f_bfree,
        expected.f_bavail, expected.f_namemax);

    struct statvfs *actual = malloc(sizeof(*actual));

    fs_ops.statfs("/", actual);
    printf(
        "Actual: bsize: %ld, blocks: %ld, free blocks: %ld\n available blocks: "
        "%ld, max name length: %ld\n",
        actual->f_bsize, actual->f_blocks, actual->f_bfree, actual->f_bavail,
        actual->f_namemax);
    ck_assert_int_eq(expected.f_bsize, actual->f_bsize);
    ck_assert_int_eq(expected.f_blocks, actual->f_blocks);
    ck_assert_int_eq(expected.f_bfree, actual->f_bfree);
    ck_assert_int_eq(expected.f_bavail, actual->f_bavail);
    ck_assert_int_eq(expected.f_namemax, actual->f_namemax);
    free(actual);
}
END_TEST

START_TEST(fschmod_file_test) {
    // {"/file.1k", 500, 500, 0100666, 1000, 1565283152, 1565283152}
    int i = 1;
    attr_t file = attr_table[i];
    mode_t new_perm = 0100222;
    printf("\nPath is %s\n Previous: permission: %o isfile:%d \n",
           attr_table[i].path, attr_table[i].mode, S_ISREG(file.mode));
    printf("chmod file permission to : %o\n", new_perm);

    fs_ops.chmod(file.path, new_perm);
    struct stat st;
    fs_ops.getattr(file.path, &st);
    printf("new attrs: permission: %o \t isfile: %d\n", st.st_mode,
           S_ISREG(st.st_mode));

    ck_assert_int_eq(new_perm, st.st_mode);
    ck_assert_int_eq(S_ISREG(file.mode), S_ISREG(st.st_mode));

    // give bad permission test
    new_perm = 0040333;
    mode_t expect_perm = 0100333;
    printf("\nPath is %s\n Previous: permission: %o isfile:%d \n",
           attr_table[i].path, attr_table[i].mode, S_ISREG(file.mode));
    printf("chmod file permission to : %o\n", new_perm);

    fs_ops.chmod(file.path, new_perm);
    fs_ops.getattr(file.path, &st);
    printf("new attrs: permission: %o \t isfile: %d\n", st.st_mode,
           S_ISREG(st.st_mode));

    ck_assert_int_eq(expect_perm, st.st_mode);
    ck_assert_int_eq(S_ISREG(file.mode), S_ISREG(st.st_mode));
}
END_TEST

START_TEST(fschmod_dir_test) {
    //    {"/dir-with-long-name", 0, 0, 040777, 4096, 1565283152, 1565283167},
    int i = 3;
    attr_t file = attr_table[i];
    mode_t new_perm = 040222;
    printf("\nPath is %s\n Previous: permission: %o isfile:%d \n",
           attr_table[i].path, attr_table[i].mode, S_ISREG(file.mode));
    printf("chmod file permission to : %o\n", new_perm);

    fs_ops.chmod(file.path, new_perm);
    struct stat st;
    fs_ops.getattr(file.path, &st);
    printf("new attrs: permission: %o \t isfile: %d\n", st.st_mode,
           S_ISREG(st.st_mode));

    ck_assert_int_eq(new_perm, st.st_mode);
    ck_assert_int_eq(S_ISREG(file.mode), S_ISREG(st.st_mode));

    // give bad permission test
    new_perm = 0100333;
    mode_t expect_perm = 040333;
    printf("\nPath is %s\n Previous: permission: %o isfile:%d \n",
           attr_table[i].path, attr_table[i].mode, S_ISREG(file.mode));
    printf("chmod file permission to : %o\n", new_perm);

    fs_ops.chmod(file.path, new_perm);
    fs_ops.getattr(file.path, &st);
    printf("new attrs: permission: %o \t isfile: %d\n", st.st_mode,
           S_ISREG(st.st_mode));

    ck_assert_int_eq(expect_perm, st.st_mode);
    ck_assert_int_eq(S_ISREG(file.mode), S_ISREG(st.st_mode));
}
END_TEST

START_TEST(fsrename_file_test) {
    // {"/dir3/subdir/file.12k", 500, 500, 0100666, 12288, 1565283152,
    // 1565283167}
    // {3243963207, 12288, "/dir3/subdir/file.12k"},
    int cksum_idx = 7;
    cksum_t cksum_entry = cksum_table[cksum_idx];
    const char *src_path = "/dir3/subdir/file.12k";
    const char *des_path = "/dir3/subdir/renamed.12k";
    printf("Rename file: %s to %s \n", cksum_entry.path, des_path);

    int status = fs_ops.rename(src_path, des_path);
    printf("status: %d\n", status);
    ck_assert_int_eq(status, 0);

    printf("Expected cksum: %u \n", cksum_entry.cksum);

    // int fs_read(const char *path, char *buf, size_t len, off_t offset,
    //         struct fuse_file_info *fi)
    char *buf = malloc(sizeof(*buf) * cksum_entry.len);

    fs_ops.read(des_path, buf, cksum_entry.len, 0, NULL);
    unsigned act_cksum = crc32(0, (unsigned char *)buf, cksum_entry.len);

    printf("Acutal cksum: %u\n", act_cksum);
    ck_assert_int_eq(cksum_entry.cksum, act_cksum);
}
END_TEST

int main(int argc, char **argv) {
    block_init("test.img");
    fs_ops.init(NULL);

    // Regenerate the test image each time of tests.
    system("python gen-disk.py -q disk1.in test.img");

    Suite *s = suite_create("fs5600");
    TCase *tc = tcase_create("read_mostly");
    TCase *tc_sample_getattr = tcase_create("get_attr sample test");
    TCase *tc_getattr = tcase_create("get_attr test");
    TCase *tc_getattr_error = tcase_create("get_attr translation error test");

    TCase *tc_readir = tcase_create("readdir test");
    TCase *tc_readdir_error = tcase_create("readdir error test");

    TCase *tc_single_read = tcase_create("single read test");
    TCase *tc_chunk_read = tcase_create("chuck read test");

    TCase *tc_fsstat = tcase_create("fs stat test");

    TCase *tc_chmod_file_test = tcase_create("fs chmod file test");
    TCase *tc_chmod_dir_test = tcase_create("fs chmod dir test");

    TCase *tc_rename_file = tcase_create("fs rename file test");

    tcase_add_test(tc, a_test); /* see START_TEST above */
    /* add more tests here */
    tcase_add_test(tc_sample_getattr, getattr_sample_test);
    tcase_add_test(tc_getattr, getattr_test);
    tcase_add_test(tc_getattr_error, getattr_error_test);
    tcase_add_test(tc_single_read, single_read_test);
    tcase_add_test(tc_readir, readdir_test);
    tcase_add_test(tc_readdir_error, readdir_error_test);
    tcase_add_test(tc_chunk_read, fsread_small_read_test);
    tcase_add_test(tc_fsstat, fsstat_test);
    tcase_add_test(tc_chmod_file_test, fschmod_file_test);
    tcase_add_test(tc_chmod_dir_test, fschmod_dir_test);

    tcase_add_test(tc_rename_file, fsrename_file_test);

    suite_add_tcase(s, tc);
    /* TODO: Uncomment below testcases one by one.*/

    suite_add_tcase(s, tc_sample_getattr);
    suite_add_tcase(s, tc_getattr);
    suite_add_tcase(s, tc_getattr_error);

    suite_add_tcase(s, tc_readir);

    suite_add_tcase(s, tc_readdir_error);
    suite_add_tcase(s, tc_single_read);
    suite_add_tcase(s, tc_chunk_read);

    suite_add_tcase(s, tc_fsstat);

    suite_add_tcase(s, tc_chmod_file_test);
    suite_add_tcase(s, tc_chmod_dir_test);
    suite_add_tcase(s, tc_rename_file);

    SRunner *sr = srunner_create(s);
    /* Note:  No fork mode cause
    suite_add_tcase(s, tc_single_read); or suite_add_tcase(s, tc_chunk_read); to
    conflict with suite_add_tcase(s, tc_readir); gives weird segfault error.
    */

    // srunner_set_fork_status(sr, CK_NOFORK);

    srunner_run_all(sr, CK_VERBOSE);
    int n_failed = srunner_ntests_failed(sr);
    printf("%d tests failed\n", n_failed);

    srunner_free(sr);
    return (n_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
