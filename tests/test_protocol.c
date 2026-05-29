#include "test_framework.h"
#include "protocol.h"
#include <string.h>

/* ── build_success_msg ─────────────────────────────────────────────────────── */

static void test_success_basic(void) {
    TLVMessage msg;
    ASSERT_EQ(build_success_msg(&msg), 0);
    ASSERT_EQ(msg.tag, MSG_SUCCESS);
    /* length is implementation-defined — just verify tag is correct */
}

static void test_success_null(void) {
    ASSERT_EQ(build_success_msg(NULL), -1);
}

/* ── build_error_msg ───────────────────────────────────────────────────────── */

static void test_error_basic(void) {
    TLVMessage msg;
    ASSERT_EQ(build_error_msg(&msg, ERR_USER_EXISTS), 0);
    ASSERT_EQ(msg.tag,      MSG_ERROR);
    ASSERT_EQ(msg.length,   1);
    ASSERT_EQ(msg.value[0], ERR_USER_EXISTS);
}

static void test_error_all_codes(void) {
    TLVMessage msg;
    ErrorCode codes[] = {
        ERR_NONE, ERR_USER_EXISTS, ERR_USER_NOT_FOUND,
        ERR_WRONG_PASSWORD, ERR_ALREADY_LOGGED_IN,
        ERR_GROUP_NOT_FOUND, ERR_GROUP_FULL,
        ERR_NOT_IN_GROUP, ERR_SERVER_FULL
    };
    for (int i = 0; i < (int)(sizeof(codes)/sizeof(codes[0])); i++) {
        ASSERT_EQ(build_error_msg(&msg, codes[i]), 0);
        ASSERT_EQ(msg.value[0], (uint8_t)codes[i]);
    }
}

static void test_error_null(void) {
    ASSERT_EQ(build_error_msg(NULL, ERR_NONE), -1);
}

/* ── build_auth_msg ────────────────────────────────────────────────────────── */

static void test_auth_register(void) {
    TLVMessage msg;
    ASSERT_EQ(build_auth_msg(&msg, MSG_REGISTER, "alice", "pass123"), 0);
    ASSERT_EQ(msg.tag,      MSG_REGISTER);
    ASSERT_EQ(msg.value[0], 5);                         /* ulen  */
    ASSERT(memcmp(msg.value + 1, "alice", 5) == 0);     /* uname */
    ASSERT_EQ(msg.value[6], 7);                         /* plen  */
    ASSERT(memcmp(msg.value + 7, "pass123", 7) == 0);   /* pass  */
    ASSERT_EQ(msg.length, 1 + 5 + 1 + 7);
}

static void test_auth_login(void) {
    TLVMessage msg;
    ASSERT_EQ(build_auth_msg(&msg, MSG_LOGIN, "bob", "secret"), 0);
    ASSERT_EQ(msg.tag, MSG_LOGIN);
}

static void test_auth_null(void) {
    TLVMessage msg;
    ASSERT_EQ(build_auth_msg(NULL, MSG_LOGIN, "u", "p"), -1);
    ASSERT_EQ(build_auth_msg(&msg, MSG_LOGIN, NULL, "p"), -1);
    ASSERT_EQ(build_auth_msg(&msg, MSG_LOGIN, "u", NULL), -1);
}

/* ── build_group_msg ───────────────────────────────────────────────────────── */

static void test_group_join(void) {
    TLVMessage msg;
    ASSERT_EQ(build_group_msg(&msg, MSG_JOIN_GROUP, "mygroup"), 0);
    ASSERT_EQ(msg.tag,      MSG_JOIN_GROUP);
    ASSERT_EQ(msg.value[0], 7);
    ASSERT(memcmp(msg.value + 1, "mygroup", 7) == 0);
    ASSERT_EQ(msg.length, 1 + 7);
}

static void test_group_create(void) {
    TLVMessage msg;
    ASSERT_EQ(build_group_msg(&msg, MSG_CREATE_GROUP, "chat"), 0);
    ASSERT_EQ(msg.tag, MSG_CREATE_GROUP);
}

static void test_group_leave(void) {
    TLVMessage msg;
    ASSERT_EQ(build_group_msg(&msg, MSG_LEAVE_GROUP, "mygroup"), 0);
    ASSERT_EQ(msg.tag, MSG_LEAVE_GROUP);
}

static void test_group_null(void) {
    TLVMessage msg;
    ASSERT_EQ(build_group_msg(NULL, MSG_JOIN_GROUP, "g"), -1);
    ASSERT_EQ(build_group_msg(&msg, MSG_JOIN_GROUP, NULL), -1);
}

/* ── build_simple_msg ──────────────────────────────────────────────────────── */

static void test_simple_logout(void) {
    TLVMessage msg;
    ASSERT_EQ(build_simple_msg(&msg, MSG_LOGOUT), 0);
    ASSERT_EQ(msg.tag,    MSG_LOGOUT);
    ASSERT_EQ(msg.length, 0);
}

static void test_simple_exit(void) {
    TLVMessage msg;
    ASSERT_EQ(build_simple_msg(&msg, MSG_EXIT), 0);
    ASSERT_EQ(msg.tag,    MSG_EXIT);
    ASSERT_EQ(msg.length, 0);
}

static void test_simple_null(void) {
    ASSERT_EQ(build_simple_msg(NULL, MSG_EXIT), -1);
}

/* ── build_group_info_msg / parse_group_info ───────────────────────────────── */

static void test_group_info_roundtrip(void) {
    TLVMessage msg;
    char     ip[16];
    uint16_t port;
    ASSERT_EQ(build_group_info_msg(&msg, "224.0.0.5", 5000), 0);
    ASSERT_EQ(msg.tag, MSG_GROUP_INFO);
    ASSERT_EQ(parse_group_info(&msg, ip, &port), 0);
    ASSERT_STR_EQ(ip, "224.0.0.5");
    ASSERT_EQ(port, 5000);
}

static void test_group_info_null(void) {
    TLVMessage msg;
    char     ip[16];
    uint16_t port;
    ASSERT_EQ(build_group_info_msg(NULL, "224.0.0.1", 5000), -1);
    ASSERT_EQ(build_group_info_msg(&msg, NULL, 5000), -1);
    build_group_info_msg(&msg, "224.0.0.1", 5000);
    ASSERT_EQ(parse_group_info(NULL, ip, &port), -1);
    ASSERT_EQ(parse_group_info(&msg, NULL, &port), -1);
    ASSERT_EQ(parse_group_info(&msg, ip, NULL), -1);
}

/* ── serialize / deserialize ───────────────────────────────────────────────── */

static void test_serialize_deserialize_roundtrip(void) {
    TLVMessage orig, out;
    uint8_t buf[258];
    uint8_t len;
    ASSERT_EQ(build_error_msg(&orig, ERR_WRONG_PASSWORD), 0);
    ASSERT_EQ(serialize_msg(&orig, buf, &len), 0);
    ASSERT_EQ(len, 3); /* 1 tag + 1 length byte + 1 value byte */
    ASSERT_EQ(deserialize_msg(buf, len, &out), 0);
    ASSERT_EQ(out.tag,      MSG_ERROR);
    ASSERT_EQ(out.length,   1);
    ASSERT_EQ(out.value[0], ERR_WRONG_PASSWORD);
}

static void test_serialize_no_value(void) {
    TLVMessage orig, out;
    uint8_t buf[258];
    uint8_t len;
    /* MSG_LOGOUT is confirmed to have length=0 */
    ASSERT_EQ(build_simple_msg(&orig, MSG_LOGOUT), 0);
    ASSERT_EQ(serialize_msg(&orig, buf, &len), 0);
    ASSERT_EQ(len, 2); /* header only: 1 tag + 1 length byte */
    ASSERT_EQ(deserialize_msg(buf, len, &out), 0);
    ASSERT_EQ(out.tag,    MSG_LOGOUT);
    ASSERT_EQ(out.length, 0);
}

static void test_serialize_null(void) {
    TLVMessage msg;
    uint8_t buf[258];
    uint8_t len;
    build_success_msg(&msg);
    ASSERT_EQ(serialize_msg(NULL,  buf, &len), -1);
    ASSERT_EQ(serialize_msg(&msg, NULL, &len), -1);
    ASSERT_EQ(serialize_msg(&msg,  buf, NULL), -1);
}

/* ── suite entry point ─────────────────────────────────────────────────────── */

void run_protocol_tests(void) {
    TEST_SUITE("Protocol");
    RUN_TEST(test_success_basic);
    RUN_TEST(test_success_null);
    RUN_TEST(test_error_basic);
    RUN_TEST(test_error_all_codes);
    RUN_TEST(test_error_null);
    RUN_TEST(test_auth_register);
    RUN_TEST(test_auth_login);
    RUN_TEST(test_auth_null);
    RUN_TEST(test_group_join);
    RUN_TEST(test_group_create);
    RUN_TEST(test_group_leave);
    RUN_TEST(test_group_null);
    RUN_TEST(test_simple_logout);
    RUN_TEST(test_simple_exit);
    RUN_TEST(test_simple_null);
    RUN_TEST(test_group_info_roundtrip);
    RUN_TEST(test_group_info_null);
    RUN_TEST(test_serialize_deserialize_roundtrip);
    RUN_TEST(test_serialize_no_value);
    RUN_TEST(test_serialize_null);
}
