#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// ── parsing helpers ──────────────────────────────────────────────────────────

// parse [ulen(1) | username | plen(1) | password] from a TLV value
static int parse_auth_value(const TLVMessage *_msg,
                            char _uname[MAX_USERNAME_LEN],
                            char _pass[MAX_PASSWORD_LEN]) {
    if (!_msg || _msg->length < 2) return -1;
    const uint8_t *v = _msg->value;
    uint8_t ulen = v[0];
    int pos = 1 + (int)ulen;
    if (pos + 1 > (int)_msg->length) return -1;
    uint8_t plen = v[pos];
    if (pos + 1 + (int)plen != (int)_msg->length) return -1;
    if (ulen == 0 || ulen >= MAX_USERNAME_LEN) return -1;
    if (plen == 0 || plen >= MAX_PASSWORD_LEN) return -1;
    memcpy(_uname, v + 1, ulen);
    _uname[ulen] = '\0';
    memcpy(_pass, v + pos + 1, plen);
    _pass[plen] = '\0';
    return 0;
}

// parse [glen(1) | group_name] from a TLV value
static int parse_group_value(const TLVMessage *_msg,
                             char _gname[MAX_GROUP_NAME_LEN]) {
    if (!_msg || _msg->length < 1) return -1;
    uint8_t glen = _msg->value[0];
    if (1 + (int)glen != (int)_msg->length) return -1;
    if (glen == 0 || glen >= MAX_GROUP_NAME_LEN) return -1;
    memcpy(_gname, _msg->value + 1, glen);
    _gname[glen] = '\0';
    return 0;
}

// ── fd / user lookup helpers ─────────────────────────────────────────────────

// list action: returns 0 (stop) when element matches the fd in _ctx
static int match_fd(void *_elem, void *_ctx) {
    return (int)(intptr_t)_elem != *(int *)_ctx;
}

typedef struct { int fd; UserRecord *found; } FindByFdCtx;

static int find_by_fd_action(const void *_key, void *_val, void *_ctx) {
    (void)_key;
    UserRecord  *u = (UserRecord *)_val;
    FindByFdCtx *c = (FindByFdCtx *)_ctx;
    if (u->socket_fd == c->fd) { c->found = u; return 0; }
    return 1;
}

// find the UserRecord that is currently connected on _fd
static UserRecord *find_user_by_fd(ServerContext *_ctx, int _fd) {
    FindByFdCtx c = { _fd, NULL };
    HashMap_ForEach(_ctx->user_map, find_by_fd_action, &c);
    return c.found;
}

// ── client cleanup ───────────────────────────────────────────────────────────

typedef struct {
    int  fd;
    char empty_names[MAX_GROUPS][MAX_GROUP_NAME_LEN];
    int  n_empty;
} CleanupCtx;

// for each group: remove fd from its member list; if the group is now empty, remember its name
static int cleanup_group_action(const void *_key, void *_val, void *_ctx) {
    (void)_key;
    GroupRecord *grp = (GroupRecord *)_val;
    CleanupCtx  *c   = (CleanupCtx *)_ctx;
    ListItr end = ListItrEnd(grp->members);
    ListItr itr = ListItrForEach(ListItrBegin(grp->members), end, match_fd, &c->fd);
    if (itr != end) {
        ListItrRemove(itr);
        grp->member_count--;
        if (grp->member_count == 0 && c->n_empty < MAX_GROUPS) {
            strncpy(c->empty_names[c->n_empty], grp->name, MAX_GROUP_NAME_LEN - 1);
            c->empty_names[c->n_empty][MAX_GROUP_NAME_LEN - 1] = '\0';
            c->n_empty++;
        }
    }
    return 1;
}

// remove an empty group from the map and return its multicast address to the pool
static void close_empty_group(ServerContext *_ctx, const char *_name) {
    void *pkey = NULL, *pval = NULL;
    if (HashMap_Remove(_ctx->group_map, _name, &pkey, &pval) != MAP_SUCCESS) return;
    GroupRecord *grp = (GroupRecord *)pval;
    McastEntry  *me  = (McastEntry *)malloc(sizeof(McastEntry));
    if (me) {
        strncpy(me->ip, grp->mcast_ip, sizeof(me->ip) - 1);
        me->ip[sizeof(me->ip) - 1] = '\0';
        me->port = grp->mcast_port;
        QueueInsert(_ctx->mcast_queue, me);
    }
    if (grp->members) ListDestroy(&grp->members, NULL);
    free(grp);
    printf("[server] closed empty group '%s'\n", _name);
}

// remove _fd from all groups it is in, close any that became empty, mark user inactive
static void cleanup_client(ServerContext *_ctx, int _fd) {
    CleanupCtx c;
    memset(&c, 0, sizeof(c));
    c.fd = _fd;
    HashMap_ForEach(_ctx->group_map, cleanup_group_action, &c);
    for (int i = 0; i < c.n_empty; i++)
        close_empty_group(_ctx, c.empty_names[i]);
    UserRecord *u = find_user_by_fd(_ctx, _fd);
    if (u) { u->logged_in = 0; u->socket_fd = -1; }
}

// ── message handlers ─────────────────────────────────────────────────────────

static void handle_register(ServerContext *_ctx, int _fd, const TLVMessage *_msg) {
    TLVMessage reply;
    if (!_ctx || !_msg) return;

    char uname[MAX_USERNAME_LEN], pass[MAX_PASSWORD_LEN];
    if (parse_auth_value(_msg, uname, pass) != 0) {
        build_error_msg(&reply, ERR_USER_NOT_FOUND);
        net_send_msg(_fd, &reply); return;
    }

    void *dummy = NULL;
    if (HashMap_Find(_ctx->user_map, uname, &dummy) == MAP_SUCCESS) {
        build_error_msg(&reply, ERR_USER_EXISTS);
        net_send_msg(_fd, &reply); return;
    }

    UserRecord *u = (UserRecord *)calloc(1, sizeof(UserRecord));
    if (!u) {
        build_error_msg(&reply, ERR_SERVER_FULL);
        net_send_msg(_fd, &reply); return;
    }
    strncpy(u->username, uname, MAX_USERNAME_LEN - 1);
    strncpy(u->password, pass,  MAX_PASSWORD_LEN - 1);
    u->logged_in = 0;
    u->socket_fd = -1;

    if (HashMap_Insert(_ctx->user_map, u->username, u) != MAP_SUCCESS) {
        free(u);
        build_error_msg(&reply, ERR_SERVER_FULL);
        net_send_msg(_fd, &reply); return;
    }
    printf("[server] registered '%s'\n", u->username);
    build_success_msg(&reply);
    net_send_msg(_fd, &reply);
}

static void handle_login(ServerContext *_ctx, int _fd, const TLVMessage *_msg) {
    TLVMessage reply;
    if (!_ctx || !_msg) return;

    char uname[MAX_USERNAME_LEN], pass[MAX_PASSWORD_LEN];
    if (parse_auth_value(_msg, uname, pass) != 0) {
        build_error_msg(&reply, ERR_USER_NOT_FOUND);
        net_send_msg(_fd, &reply); return;
    }

    UserRecord *u = NULL;
    if (HashMap_Find(_ctx->user_map, uname, (void **)&u) != MAP_SUCCESS) {
        build_error_msg(&reply, ERR_USER_NOT_FOUND);
        net_send_msg(_fd, &reply); return;
    }
    if (strcmp(u->password, pass) != 0) {
        build_error_msg(&reply, ERR_WRONG_PASSWORD);
        net_send_msg(_fd, &reply); return;
    }
    if (u->logged_in) {
        build_error_msg(&reply, ERR_ALREADY_LOGGED_IN);
        net_send_msg(_fd, &reply); return;
    }
    u->logged_in = 1;
    u->socket_fd = _fd;
    printf("[server] '%s' logged in on fd=%d\n", u->username, _fd);
    build_success_msg(&reply);
    net_send_msg(_fd, &reply);
}

static void handle_logout(ServerContext *_ctx, int _fd) {
    TLVMessage reply;
    if (!_ctx) return;
    cleanup_client(_ctx, _fd);
    printf("[server] fd=%d logged out\n", _fd);
    build_success_msg(&reply);
    net_send_msg(_fd, &reply);
}

static void handle_exit(ServerContext *_ctx, int _fd) {
    TLVMessage reply;
    if (!_ctx) return;
    cleanup_client(_ctx, _fd);
    build_success_msg(&reply);
    net_send_msg(_fd, &reply);
}

static void handle_join_group(ServerContext *_ctx, int _fd, const TLVMessage *_msg) {
    TLVMessage reply;
    if (!_ctx || !_msg) return;

    // only logged-in users can join groups
    UserRecord *u = find_user_by_fd(_ctx, _fd);
    if (!u || !u->logged_in) {
        build_error_msg(&reply, ERR_USER_NOT_FOUND);
        net_send_msg(_fd, &reply); return;
    }

    char gname[MAX_GROUP_NAME_LEN];
    if (parse_group_value(_msg, gname) != 0) {
        build_error_msg(&reply, ERR_GROUP_NOT_FOUND);
        net_send_msg(_fd, &reply); return;
    }

    GroupRecord *grp = NULL;
    if (HashMap_Find(_ctx->group_map, gname, (void **)&grp) != MAP_SUCCESS) {
        // group doesn't exist — create it
        if (QueueIsEmpty(_ctx->mcast_queue)) {
            build_error_msg(&reply, ERR_SERVER_FULL);
            net_send_msg(_fd, &reply); return;
        }
        McastEntry *me = NULL;
        QueueRemove(_ctx->mcast_queue, (void **)&me);

        grp = (GroupRecord *)calloc(1, sizeof(GroupRecord));
        if (!grp) {
            QueueInsert(_ctx->mcast_queue, me);
            build_error_msg(&reply, ERR_SERVER_FULL);
            net_send_msg(_fd, &reply); return;
        }
        strncpy(grp->name,     gname,  MAX_GROUP_NAME_LEN - 1);
        strncpy(grp->mcast_ip, me->ip, sizeof(grp->mcast_ip) - 1);
        grp->mcast_ip[sizeof(grp->mcast_ip) - 1] = '\0';
        grp->mcast_port   = me->port;
        grp->members      = ListCreate();
        grp->member_count = 0;

        if (!grp->members) {
            QueueInsert(_ctx->mcast_queue, me);
            free(grp);
            build_error_msg(&reply, ERR_SERVER_FULL);
            net_send_msg(_fd, &reply); return;
        }
        if (HashMap_Insert(_ctx->group_map, grp->name, grp) != MAP_SUCCESS) {
            QueueInsert(_ctx->mcast_queue, me);
            ListDestroy(&grp->members, NULL);
            free(grp);
            build_error_msg(&reply, ERR_SERVER_FULL);
            net_send_msg(_fd, &reply); return;
        }
        free(me);
        printf("[server] created group '%s' at %s:%u\n", grp->name, grp->mcast_ip, grp->mcast_port);
    }

    // add fd only if not already a member (idempotent)
    ListItr end = ListItrEnd(grp->members);
    ListItr itr = ListItrForEach(ListItrBegin(grp->members), end, match_fd, &_fd);
    if (itr == end) {
        if (grp->member_count >= MAX_MEMBERS_PER_GROUP) {
            build_error_msg(&reply, ERR_GROUP_FULL);
            net_send_msg(_fd, &reply); return;
        }
        ListPushTail(grp->members, (void *)(intptr_t)_fd);
        grp->member_count++;
        printf("[server] '%s' joined '%s'\n", u->username, grp->name);
    }

    build_group_info_msg(&reply, grp->mcast_ip, grp->mcast_port);
    net_send_msg(_fd, &reply);
}

static void handle_leave_group(ServerContext *_ctx, int _fd, const TLVMessage *_msg) {
    TLVMessage reply;
    if (!_ctx || !_msg) return;

    char gname[MAX_GROUP_NAME_LEN];
    if (parse_group_value(_msg, gname) != 0) {
        build_error_msg(&reply, ERR_GROUP_NOT_FOUND);
        net_send_msg(_fd, &reply); return;
    }

    GroupRecord *grp = NULL;
    if (HashMap_Find(_ctx->group_map, gname, (void **)&grp) != MAP_SUCCESS) {
        build_error_msg(&reply, ERR_GROUP_NOT_FOUND);
        net_send_msg(_fd, &reply); return;
    }

    ListItr end = ListItrEnd(grp->members);
    ListItr itr = ListItrForEach(ListItrBegin(grp->members), end, match_fd, &_fd);
    if (itr == end) {
        build_error_msg(&reply, ERR_NOT_IN_GROUP);
        net_send_msg(_fd, &reply); return;
    }
    ListItrRemove(itr);
    grp->member_count--;

    if (grp->member_count == 0)
        close_empty_group(_ctx, gname); // grp is freed inside — don't touch it after this

    printf("[server] fd=%d left '%s'\n", _fd, gname);
    build_success_msg(&reply);
    net_send_msg(_fd, &reply);
}

// ── public API ────────────────────────────────────────────────────────────────

int dispatch_message(ServerContext *_ctx, int _fd, const TLVMessage *_msg) {
    if (!_ctx || !_msg) return 0;
    switch ((MessageTag)_msg->tag) {
        case MSG_REGISTER:    handle_register(_ctx, _fd, _msg);    return 0;
        case MSG_LOGIN:       handle_login(_ctx, _fd, _msg);       return 0;
        case MSG_LOGOUT:      handle_logout(_ctx, _fd);            return 0;
        case MSG_EXIT:        handle_exit(_ctx, _fd);              return -1;
        case MSG_JOIN_GROUP:  handle_join_group(_ctx, _fd, _msg);  return 0;
        case MSG_LEAVE_GROUP: handle_leave_group(_ctx, _fd, _msg); return 0;
        default: {
            TLVMessage err;
            build_error_msg(&err, ERR_NONE);
            net_send_msg(_fd, &err);
            return 0;
        }
    }
}

void client_disconnected(ServerContext *_ctx, int _fd) {
    if (!_ctx) return;
    printf("[server] fd=%d disconnected unexpectedly\n", _fd);
    cleanup_client(_ctx, _fd);
}
