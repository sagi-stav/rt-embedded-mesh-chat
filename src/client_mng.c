#include "client.h"
#include "protocol.h"
#include "limits.h"
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>   /* close()  */
#include <signal.h>   /* kill()   */
#include <stdlib.h>   /* exit()   */
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>

/* ─── Static Forward Declarations ────────────────────────────────────────── */
static void close_all_group_sessions(ClientContext *_ctx);
static void clear_client_context(ClientContext *_ctx);

/* ═══════════════════════════════════════════════════════════════════════════ */
/*                          PUBLIC API                                         */
/* ═══════════════════════════════════════════════════════════════════════════ */

int client_logout(ClientContext *_ctx)
{
    TLVMessage request;
    TLVMessage response;

    if (!_ctx || _ctx->state != CLIENT_STATE_LOGGED_IN)
    {
        return -1;
    }

    /* Step 1: Notify server */
    if (build_simple_msg(&request, MSG_LOGOUT) != 0)
    {
        return -1;
    }

    if (net_send_recv(_ctx, &request, &response) != 0)
    {
        fprintf(stderr, "[-] Logout: server communication failed.\n");
        /* Continue teardown regardless */
    }

    if (response.tag != (uint8_t)MSG_SUCCESS)
    {
        fprintf(stderr, "[-] Logout: server returned error.\n");
        /* Continue teardown regardless */
    }

    /* Step 2: Kill all open chat windows (sender/receiver processes) */
    close_all_group_sessions(_ctx);

    /* Step 3: Clear session state — keep TCP open, return to Screen 1 */
    clear_client_context(_ctx);
    _ctx->state = CLIENT_STATE_CONNECTED;

    return 0;
}

void client_exit(ClientContext *_ctx)
{
    TLVMessage request;
    TLVMessage response;

    if (!_ctx)
    {
        exit(1);
    }

    /* Step 1: Notify server if connected */
    if (_ctx->state != CLIENT_STATE_DISCONNECTED && _ctx->server_fd >= 0)
    {
        if (build_simple_msg(&request, MSG_EXIT) == 0)
        {
            /* Best-effort send — ignore errors on exit path */
            net_send_recv(_ctx, &request, &response);
        }
    }

    /* Step 2: Kill all open chat windows */
    close_all_group_sessions(_ctx);

    /* Step 3: Close TCP socket */
    net_disconnect(_ctx);

    /* Step 4: Clear all context memory */
    clear_client_context(_ctx);

    printf("[*] Goodbye!\n");
    exit(0);
}

void client_close_group_session(GroupSession *_session)
{
    if (!_session)
    {
        return;
    }

    /* Kill sender process if alive */
    if (_session->sender_pid > 0)
    {
        kill(_session->sender_pid, SIGTERM);
        _session->sender_pid = 0;
    }

    /* Kill receiver process if alive */
    if (_session->receiver_pid > 0)
    {
        kill(_session->receiver_pid, SIGTERM);
        _session->receiver_pid = 0;
    }

    /* Clear session data */
    memset(_session, 0, sizeof(GroupSession));
}

int client_spawn_chat_windows(ClientContext *_ctx, GroupSession *_session) {
    char cmd[512];
    int mq_id = msgget(IPC_MSG_KEY, 0666 | IPC_CREAT);

    if (mq_id < 0)
    {
        perror("[-] client_spawn: msgget failed");
        return -1;
    }

    /* Flush the queue from previous runs before spawning new processes */
    IpcPidMessage flush_msg;
    while (msgrcv(mq_id, &flush_msg, sizeof(flush_msg) - sizeof(long), 1, IPC_NOWAIT) > 0);

    /* 1. Spawn Receiver (Top Window - Chat History) */
    snprintf(cmd, sizeof(cmd), "gnome-terminal --geometry=80x20 -- ./receiver.out %s %s",
             _session->multicast_ip, _ctx->username);
    if (system(cmd) == -1) {
        perror("[-] system() failed for receiver");
    }
    /* 2. Spawn Sender (Bottom Window - Input Area) */
    snprintf(cmd, sizeof(cmd), "gnome-terminal --geometry=80x6 -- ./sender.out %s %s",
             _session->multicast_ip, _ctx->username);
    if (system(cmd) == -1) {
        perror("[-] system() failed for sender");
    }
    /* 3. Block and wait for exactly 2 PID reports */
    IpcPidMessage msg;
    int received = 0;
    while (received < 2)
    {
        /* msgrcv blocks (waits) until a message arrives */
        if (msgrcv(mq_id, &msg, sizeof(msg) - sizeof(long), 1, 0) > 0)
        {
            if (msg.is_sender)
            {
                _session->sender_pid = msg.process_pid;
            }
            else
            {
                _session->receiver_pid = msg.process_pid;
            }
            received++;
        }
    }

    printf("[+] IPC Linked: Sender PID=%d, Receiver PID=%d\n",
           _session->sender_pid, _session->receiver_pid);

    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*                          STATIC HELPERS                                     */
/* ═══════════════════════════════════════════════════════════════════════════ */

static void close_all_group_sessions(ClientContext *_ctx)
{
    int i;
    for (i = 0; i < _ctx->active_group_count; i++)
    {
        client_close_group_session(&_ctx->active_groups[i]);
    }
    _ctx->active_group_count = 0;
}

static void clear_client_context(ClientContext *_ctx)
{
    memset(_ctx->username,      0, sizeof(_ctx->username));
    memset(_ctx->active_groups, 0, sizeof(_ctx->active_groups));
    _ctx->active_group_count = 0;
}