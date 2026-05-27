#ifndef __CLIENT_H__
#define __CLIENT_H__

#include "protocol.h"
#include "limits.h"
#include <sys/types.h>
/* ─── Client State ────────────────────────────────────────────────────────── */
typedef enum {
    CLIENT_STATE_DISCONNECTED = 0,
    CLIENT_STATE_CONNECTED,        /* TCP open, not logged in  */
    CLIENT_STATE_LOGGED_IN         /* Authenticated, can join groups */
} ClientState;

/* ─── Active Group Info (received from server on join) ───────────────────── */
typedef struct {
    char     group_name[MAX_GROUP_NAME_LEN];
    char     multicast_ip[16];
    uint16_t multicast_port;
    pid_t    sender_pid;
    pid_t    receiver_pid;
} GroupSession;

/* ─── Client Context (single global instance) ────────────────────────────── */
typedef struct {
    int          server_fd;                          /* TCP socket to server  */
    ClientState  state;
    char         username[MAX_USERNAME_LEN];
    GroupSession active_groups[MAX_GROUPS];
    int          active_group_count;
} ClientContext;

/* ─── UI API (client_ui.c) ───────────────────────────────────────────────── */

/**
 * @brief Run Screen 1: Registration / Login / Exit loop.
 * @param[in] _ctx - Client context
 * Blocks until user successfully logs in or chooses Exit.
 */
void ui_run_screen1(ClientContext *_ctx);

/**
 * @brief Run Screen 2: Group management loop (post-login).
 * @param[in] _ctx - Client context
 * Blocks until user logs out.
 */
void ui_run_screen2(ClientContext *_ctx);

/* ─── Network API (client_net.c) — stubs for now ────────────────────────── */

/**
 * @brief Connect TCP socket to server.
 * @return 0 on success, -1 on failure
 */
int  net_connect(ClientContext *_ctx);

/**
 * @brief Send a TLVMessage to the server and receive the response.
 * @param[in]  _ctx      - Client context
 * @param[in]  _request  - Message to send
 * @param[out] _response - Response from server
 * @return 0 on success, -1 on failure
 */
int  net_send_recv(ClientContext *_ctx, const TLVMessage *_request,
                   TLVMessage *_response);

/**
 * @brief Gracefully close the TCP connection.
 */
void net_disconnect(ClientContext *_ctx);

/* ─── Management API (client_mng.c) ──────────────────────────────────────── */

/**
 * @brief Logout: notify server, kill all group windows, clear context.
 * @param[in] _ctx - Client context
 * @return 0 on success, -1 on failure
 */
int client_logout(ClientContext *_ctx);

/**
 * @brief Exit: send exit msg to server, close TCP, clear context, terminate.
 * @param[in] _ctx - Client context
 */
void client_exit(ClientContext *_ctx);

/**
 * @brief Close a single group session (kill sender/receiver PIDs).
 * @param[in] _session - GroupSession to close
 * @note PIDs teardown requires IPC (Epic 4); stubbed for now.
 */
void client_close_group_session(GroupSession *_session);

#endif /* __CLIENT_H__ */