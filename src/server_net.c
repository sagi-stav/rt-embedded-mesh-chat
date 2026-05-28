#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

/* ══════════════════════════════════════════════════════════════════════════
 *  Static helpers
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * djb2 hash for string keys used in both HashMaps.
 * Same algorithm used in server_mng.c for password hashing.
 */
static size_t hash_string(void *_key) {
    const char *str = (const char *) _key;
    size_t hash = 5381;
    int c;
    while ((c = (unsigned char) *str++))
        hash = ((hash << 5) + hash) ^ (size_t) c;
    return hash;
}

/** String equality for HashMap key comparisons. */
static int equal_string(void *_a, void *_b) {
    return strcmp((const char *) _a, (const char *) _b) == 0;
}

/** Destroy callback for user_map values (UserRecord*). */
static void destroy_user_record(void *_val) {
    free(_val);
}

/** Destroy callback for group_map values (GroupRecord*). */
static void destroy_group_record(void *_val) {
    GroupRecord *g = (GroupRecord *) _val;
    if (g->members)
        ListDestroy(&g->members, NULL); /* fds stored as cast — no heap per member */
    free(g);
}

/** Destroy callback for mcast_queue items (McastEntry*). */
static void destroy_mcast_entry(void *_item) {
    free(_item);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Signal handling
 * ══════════════════════════════════════════════════════════════════════════ */

static ServerContext *g_ctx = NULL; /* for signal handler access */

static void handle_shutdown(int _sig) {
    (void) _sig;
    if (g_ctx)
        g_ctx->running = 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  server_create
 * ══════════════════════════════════════════════════════════════════════════ */

ServerContext *server_create(void) {
    ServerContext *ctx = (ServerContext *) calloc(1, sizeof(ServerContext));
    if (!ctx) return NULL;

    ctx->listen_fd = -1;
    ctx->running = 1;

    /* All client slots start empty */
    for (int i = 0; i < MAX_CLIENTS; i++)
        ctx->client_fds[i] = -1;

    /* User HashMap: key = username string, value = UserRecord* */
    ctx->user_map = HashMap_Create(MAX_CLIENTS, hash_string, equal_string);
    if (!ctx->user_map) goto fail;

    /* Group HashMap: key = group name string, value = GroupRecord* */
    ctx->group_map = HashMap_Create(MAX_GROUPS, hash_string, equal_string);
    if (!ctx->group_map) goto fail;

    /* Multicast address pool */
    ctx->mcast_queue = QueueCreate(MULTICAST_POOL_SIZE);
    if (!ctx->mcast_queue) goto fail;

    /* Pre-load pool: 224.0.0.1:5000  through  224.0.0.{N}:{5000+N-1} */
    uint32_t base_addr = ntohl(inet_addr(MULTICAST_BASE_IP));
    for (int i = 0; i < MULTICAST_POOL_SIZE; i++) {
        McastEntry *entry = (McastEntry *) malloc(sizeof(McastEntry));
        if (!entry) goto fail;

        uint32_t addr = htonl(base_addr + (uint32_t)i);
        inet_ntop(AF_INET, &addr, entry->ip, sizeof(entry->ip));
        entry->port = (uint16_t) (MULTICAST_BASE_PORT + i);

        if (QueueInsert(ctx->mcast_queue, entry) != QUEUE_SUCCESS) {
            free(entry);
            goto fail;
        }
    }

    return ctx;

fail:
    server_destroy(ctx);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  server_destroy
 * ══════════════════════════════════════════════════════════════════════════ */

void server_destroy(ServerContext *_ctx) {
    if (!_ctx) return;

    /* Close listening socket */
    if (_ctx->listen_fd >= 0)
        close(_ctx->listen_fd);

    /* Close all active client sockets */
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (_ctx->client_fds[i] >= 0)
            close(_ctx->client_fds[i]);

    /* Keys are pointers into the freed values — pass NULL for key destroyer */
    if (_ctx->user_map)
        HashMap_Destroy(&_ctx->user_map, NULL, destroy_user_record);
    if (_ctx->group_map)
        HashMap_Destroy(&_ctx->group_map, NULL, destroy_group_record);
    if (_ctx->mcast_queue)
        QueueDestroy(&_ctx->mcast_queue, destroy_mcast_entry);

    free(_ctx);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  net_read_msg
 * ══════════════════════════════════════════════════════════════════════════ */

int net_read_msg(int _fd, TLVMessage *_msg) {
    /* Read 2-byte header: [tag][length] */
    uint8_t header[2];
    ssize_t n = 0;
    while (n < 2) {
        ssize_t r = read(_fd, header + n, (size_t) (2 - n));
        if (r <= 0)
            return -1; /* connection closed or error */
        n += r;
    }

    _msg->tag = header[0];
    _msg->length = header[1];

    if (_msg->length == 0)
        return 0; /* no value bytes to read */

    /* Read value bytes */
    n = 0;
    while (n < (ssize_t) _msg->length) {
        ssize_t r = read(_fd, _msg->value + n, (size_t) (_msg->length - n));
        if (r <= 0)
            return -1;
        n += r;
    }
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  net_send_msg
 * ══════════════════════════════════════════════════════════════════════════ */

int net_send_msg(int _fd, const TLVMessage *_msg) {
    uint8_t buf[2 + TLV_MAX_VALUE_LEN];
    uint8_t out_len = 0;

    if (serialize_msg(_msg, buf, &out_len) != 0)
        return -1;

    ssize_t sent = 0;
    while (sent < (ssize_t) out_len) {
        ssize_t w = write(_fd, buf + sent, (size_t) (out_len - sent));
        if (w <= 0)
            return -1;
        sent += w;
    }
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  server_run
 * ══════════════════════════════════════════════════════════════════════════ */

void server_run(ServerContext *_ctx) {
    /* Register signal handlers so Ctrl-C shuts down cleanly */
    g_ctx = _ctx;
    signal(SIGINT, handle_shutdown);
    signal(SIGTERM, handle_shutdown);

    /* Create TCP listening socket */
    _ctx->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_ctx->listen_fd < 0) {
        perror("socket");
        return;
    }

    int opt = 1;
    setsockopt(_ctx->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(SERVER_PORT);

    if (bind(_ctx->listen_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("bind");
        close(_ctx->listen_fd);
        return;
    }
    if (listen(_ctx->listen_fd, SERVER_BACKLOG) < 0) {
        perror("listen");
        close(_ctx->listen_fd);
        return;
    }
    printf("[server] Listening on port %d\n", SERVER_PORT);

    /* ── Main select() loop ─────────────────────────────────────────────── */
    while (_ctx->running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(_ctx->listen_fd, &read_fds);
        int max_fd = _ctx->listen_fd;

        /* Add every active client fd to the watch set */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (_ctx->client_fds[i] >= 0) {
                FD_SET(_ctx->client_fds[i], &read_fds);
                if (_ctx->client_fds[i] > max_fd)
                    max_fd = _ctx->client_fds[i];
            }
        }

        int ready = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (ready < 0) {
            if (errno == EINTR) continue; /* woken by signal — recheck running */
            perror("select");
            break;
        }

        /* ── New incoming connection ──────────────────────────────────── */
        if (FD_ISSET(_ctx->listen_fd, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int new_fd = accept(_ctx->listen_fd,
                                (struct sockaddr *) &client_addr, &client_len);
            if (new_fd >= 0) {
                /* Find a free slot */
                int slot = -1;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (_ctx->client_fds[i] < 0) {
                        slot = i;
                        break;
                    }
                }

                if (slot >= 0) {
                    _ctx->client_fds[slot] = new_fd;
                    printf("[server] New client fd=%d from %s\n",
                           new_fd, inet_ntoa(client_addr.sin_addr));
                } else {
                    /* No room — tell the client and drop the connection */
                    TLVMessage err;
                    build_error_msg(&err, ERR_SERVER_FULL);
                    net_send_msg(new_fd, &err);
                    close(new_fd);
                    printf("[server] Rejected client (server full)\n");
                }
            }
        }

        /* ── Data from existing clients ───────────────────────────────── */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int fd = _ctx->client_fds[i];
            if (fd < 0 || !FD_ISSET(fd, &read_fds))
                continue;

            TLVMessage msg;
            if (net_read_msg(fd, &msg) != 0) {
                /* EOF or error — client disconnected unexpectedly */
                printf("[server] Client fd=%d disconnected\n", fd);
                client_disconnected(_ctx, fd);
                close(fd);
                _ctx->client_fds[i] = -1;
            } else {
                /* dispatch_message returns -1 when MSG_EXIT is received */
                if (dispatch_message(_ctx, fd, &msg) < 0) {
                    close(fd);
                    _ctx->client_fds[i] = -1;
                }
            }
        }
    }

    printf("[server] Shutting down.\n");
}
