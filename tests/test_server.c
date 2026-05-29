#include "test_framework.h"
#include "server.h"
#include "protocol.h"
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

/*
 * socketpair() creates two connected sockets without needing a real network.
 * fds[0] = "client" side  (test reads/writes here)
 * fds[1] = "server" side  (passed to dispatch_message as the client fd)
 */
static int make_pair(int fds[2]) {
    return socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
}

/* ── null / edge cases ─────────────────────────────────────────────────────── */

static void test_dispatch_null(void) {
    ASSERT_EQ(dispatch_message(NULL, -1, NULL), 0);
}

/* ── register ──────────────────────────────────────────────────────────────── */

static void test_register_success(void) {
    ServerContext *ctx = server_create();
    ASSERT_FATAL(ctx);
    int fds[2];
    ASSERT_FATAL(make_pair(fds) == 0);

    TLVMessage req, reply;
    build_auth_msg(&req, MSG_REGISTER, "alice", "secret");
    dispatch_message(ctx, fds[1], &req);
    ASSERT_EQ(net_read_msg(fds[0], &reply), 0);
    ASSERT_EQ(reply.tag, MSG_SUCCESS);

    close(fds[0]); close(fds[1]);
    server_destroy(ctx);
}

static void test_register_duplicate(void) {
    ServerContext *ctx = server_create();
    ASSERT_FATAL(ctx);
    int fds[2];
    ASSERT_FATAL(make_pair(fds) == 0);

    TLVMessage req, reply;
    build_auth_msg(&req, MSG_REGISTER, "bob", "pass");
    dispatch_message(ctx, fds[1], &req);
    net_read_msg(fds[0], &reply);               /* consume first reply  */
    dispatch_message(ctx, fds[1], &req);        /* register again       */
    ASSERT_EQ(net_read_msg(fds[0], &reply), 0);
    ASSERT_EQ(reply.tag,      MSG_ERROR);
    ASSERT_EQ(reply.value[0], ERR_USER_EXISTS);

    close(fds[0]); close(fds[1]);
    server_destroy(ctx);
}

/* ── login ─────────────────────────────────────────────────────────────────── */

static void test_login_success(void) {
    ServerContext *ctx = server_create();
    ASSERT_FATAL(ctx);
    int fds[2];
    ASSERT_FATAL(make_pair(fds) == 0);

    TLVMessage req, reply;
    build_auth_msg(&req, MSG_REGISTER, "carol", "mypass");
    dispatch_message(ctx, fds[1], &req);
    net_read_msg(fds[0], &reply);

    build_auth_msg(&req, MSG_LOGIN, "carol", "mypass");
    dispatch_message(ctx, fds[1], &req);
    ASSERT_EQ(net_read_msg(fds[0], &reply), 0);
    ASSERT_EQ(reply.tag, MSG_SUCCESS);

    close(fds[0]); close(fds[1]);
    server_destroy(ctx);
}

static void test_login_wrong_password(void) {
    ServerContext *ctx = server_create();
    ASSERT_FATAL(ctx);
    int fds[2];
    ASSERT_FATAL(make_pair(fds) == 0);

    TLVMessage req, reply;
    build_auth_msg(&req, MSG_REGISTER, "dave", "right");
    dispatch_message(ctx, fds[1], &req);
    net_read_msg(fds[0], &reply);

    build_auth_msg(&req, MSG_LOGIN, "dave", "wrong");
    dispatch_message(ctx, fds[1], &req);
    ASSERT_EQ(net_read_msg(fds[0], &reply), 0);
    ASSERT_EQ(reply.tag,      MSG_ERROR);
    ASSERT_EQ(reply.value[0], ERR_WRONG_PASSWORD);

    close(fds[0]); close(fds[1]);
    server_destroy(ctx);
}

static void test_login_not_registered(void) {
    ServerContext *ctx = server_create();
    ASSERT_FATAL(ctx);
    int fds[2];
    ASSERT_FATAL(make_pair(fds) == 0);

    TLVMessage req, reply;
    build_auth_msg(&req, MSG_LOGIN, "ghost", "pass");
    dispatch_message(ctx, fds[1], &req);
    ASSERT_EQ(net_read_msg(fds[0], &reply), 0);
    ASSERT_EQ(reply.tag,      MSG_ERROR);
    ASSERT_EQ(reply.value[0], ERR_USER_NOT_FOUND);

    close(fds[0]); close(fds[1]);
    server_destroy(ctx);
}

static void test_login_already_logged_in(void) {
    ServerContext *ctx = server_create();
    ASSERT_FATAL(ctx);
    int fds[2];
    ASSERT_FATAL(make_pair(fds) == 0);

    TLVMessage req, reply;
    build_auth_msg(&req, MSG_REGISTER, "eve", "pw");
    dispatch_message(ctx, fds[1], &req);
    net_read_msg(fds[0], &reply);

    build_auth_msg(&req, MSG_LOGIN, "eve", "pw");
    dispatch_message(ctx, fds[1], &req);
    net_read_msg(fds[0], &reply);               /* first login: success */

    dispatch_message(ctx, fds[1], &req);        /* second login: error  */
    ASSERT_EQ(net_read_msg(fds[0], &reply), 0);
    ASSERT_EQ(reply.tag,      MSG_ERROR);
    ASSERT_EQ(reply.value[0], ERR_ALREADY_LOGGED_IN);

    close(fds[0]); close(fds[1]);
    server_destroy(ctx);
}

/* ── group management ──────────────────────────────────────────────────────── */

/* helper: register + login a user, returns 0 on success */
static int register_and_login(ServerContext *ctx, int server_fd, int client_fd,
                               const char *user, const char *pass) {
    TLVMessage req, reply;
    build_auth_msg(&req, MSG_REGISTER, user, pass);
    dispatch_message(ctx, server_fd, &req);
    if (net_read_msg(client_fd, &reply) != 0 || reply.tag != MSG_SUCCESS) return -1;
    build_auth_msg(&req, MSG_LOGIN, user, pass);
    dispatch_message(ctx, server_fd, &req);
    if (net_read_msg(client_fd, &reply) != 0 || reply.tag != MSG_SUCCESS) return -1;
    return 0;
}

static void test_create_group_success(void) {
    ServerContext *ctx = server_create();
    ASSERT_FATAL(ctx);
    int fds[2];
    ASSERT_FATAL(make_pair(fds) == 0);
    ASSERT_FATAL(register_and_login(ctx, fds[1], fds[0], "frank", "pw") == 0);

    TLVMessage req, reply;
    build_group_msg(&req, MSG_CREATE_GROUP, "g1");
    dispatch_message(ctx, fds[1], &req);
    ASSERT_EQ(net_read_msg(fds[0], &reply), 0);
    ASSERT_EQ(reply.tag, MSG_GROUP_INFO);

    char ip[16]; uint16_t port;
    ASSERT_EQ(parse_group_info(&reply, ip, &port), 0);
    ASSERT_EQ(port, 5000);

    close(fds[0]); close(fds[1]);
    server_destroy(ctx);
}

static void test_create_group_duplicate(void) {
    ServerContext *ctx = server_create();
    ASSERT_FATAL(ctx);
    int fds[2];
    ASSERT_FATAL(make_pair(fds) == 0);
    ASSERT_FATAL(register_and_login(ctx, fds[1], fds[0], "grace", "pw") == 0);

    TLVMessage req, reply;
    build_group_msg(&req, MSG_CREATE_GROUP, "dup");
    dispatch_message(ctx, fds[1], &req);
    net_read_msg(fds[0], &reply);               /* first create: success */
    dispatch_message(ctx, fds[1], &req);        /* second create: error  */
    ASSERT_EQ(net_read_msg(fds[0], &reply), 0);
    ASSERT_EQ(reply.tag, MSG_ERROR);

    close(fds[0]); close(fds[1]);
    server_destroy(ctx);
}

static void test_join_group_success(void) {
    ServerContext *ctx = server_create();
    ASSERT_FATAL(ctx);
    int fds_a[2], fds_b[2];
    ASSERT_FATAL(make_pair(fds_a) == 0);
    ASSERT_FATAL(make_pair(fds_b) == 0);
    ASSERT_FATAL(register_and_login(ctx, fds_a[1], fds_a[0], "henry", "pw") == 0);
    ASSERT_FATAL(register_and_login(ctx, fds_b[1], fds_b[0], "irene", "pw") == 0);

    TLVMessage req, reply;
    /* henry creates the group */
    build_group_msg(&req, MSG_CREATE_GROUP, "room1");
    dispatch_message(ctx, fds_a[1], &req);
    net_read_msg(fds_a[0], &reply);

    /* irene joins it */
    build_group_msg(&req, MSG_JOIN_GROUP, "room1");
    dispatch_message(ctx, fds_b[1], &req);
    ASSERT_EQ(net_read_msg(fds_b[0], &reply), 0);
    ASSERT_EQ(reply.tag, MSG_GROUP_INFO);

    close(fds_a[0]); close(fds_a[1]);
    close(fds_b[0]); close(fds_b[1]);
    server_destroy(ctx);
}

static void test_join_group_not_found(void) {
    ServerContext *ctx = server_create();
    ASSERT_FATAL(ctx);
    int fds[2];
    ASSERT_FATAL(make_pair(fds) == 0);
    ASSERT_FATAL(register_and_login(ctx, fds[1], fds[0], "jack", "pw") == 0);

    TLVMessage req, reply;
    build_group_msg(&req, MSG_JOIN_GROUP, "nonexistent");
    dispatch_message(ctx, fds[1], &req);
    ASSERT_EQ(net_read_msg(fds[0], &reply), 0);
    ASSERT_EQ(reply.tag,      MSG_ERROR);
    ASSERT_EQ(reply.value[0], ERR_GROUP_NOT_FOUND);

    close(fds[0]); close(fds[1]);
    server_destroy(ctx);
}

static void test_leave_group_success(void) {
    ServerContext *ctx = server_create();
    ASSERT_FATAL(ctx);
    int fds[2];
    ASSERT_FATAL(make_pair(fds) == 0);
    ASSERT_FATAL(register_and_login(ctx, fds[1], fds[0], "kate", "pw") == 0);

    TLVMessage req, reply;
    build_group_msg(&req, MSG_CREATE_GROUP, "temp");
    dispatch_message(ctx, fds[1], &req);
    net_read_msg(fds[0], &reply);

    build_group_msg(&req, MSG_LEAVE_GROUP, "temp");
    dispatch_message(ctx, fds[1], &req);
    ASSERT_EQ(net_read_msg(fds[0], &reply), 0);
    ASSERT_EQ(reply.tag, MSG_SUCCESS);

    close(fds[0]); close(fds[1]);
    server_destroy(ctx);
}

/* ── logout / exit ─────────────────────────────────────────────────────────── */

static void test_logout_success(void) {
    ServerContext *ctx = server_create();
    ASSERT_FATAL(ctx);
    int fds[2];
    ASSERT_FATAL(make_pair(fds) == 0);
    ASSERT_FATAL(register_and_login(ctx, fds[1], fds[0], "leo", "pw") == 0);

    TLVMessage req, reply;
    build_simple_msg(&req, MSG_LOGOUT);
    dispatch_message(ctx, fds[1], &req);
    ASSERT_EQ(net_read_msg(fds[0], &reply), 0);
    ASSERT_EQ(reply.tag, MSG_SUCCESS);

    close(fds[0]); close(fds[1]);
    server_destroy(ctx);
}

static void test_exit_returns_minus1(void) {
    ServerContext *ctx = server_create();
    ASSERT_FATAL(ctx);
    int fds[2];
    ASSERT_FATAL(make_pair(fds) == 0);

    TLVMessage req, reply;
    build_simple_msg(&req, MSG_EXIT);
    int ret = dispatch_message(ctx, fds[1], &req);
    ASSERT_EQ(ret, -1);
    net_read_msg(fds[0], &reply); /* consume success reply */

    close(fds[0]); close(fds[1]);
    server_destroy(ctx);
}

/* ── suite entry point ─────────────────────────────────────────────────────── */

void run_server_tests(void) {
    TEST_SUITE("Server dispatch");
    RUN_TEST(test_dispatch_null);
    RUN_TEST(test_register_success);
    RUN_TEST(test_register_duplicate);
    RUN_TEST(test_login_success);
    RUN_TEST(test_login_wrong_password);
    RUN_TEST(test_login_not_registered);
    RUN_TEST(test_login_already_logged_in);
    RUN_TEST(test_create_group_success);
    RUN_TEST(test_create_group_duplicate);
    RUN_TEST(test_join_group_success);
    RUN_TEST(test_join_group_not_found);
    RUN_TEST(test_leave_group_success);
    RUN_TEST(test_logout_success);
    RUN_TEST(test_exit_returns_minus1);
}
