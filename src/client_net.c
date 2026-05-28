#include "client.h"
#include "protocol.h"
#include "limits.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

/* ─── Static Forward Declarations ────────────────────────────────────────── */
static int  send_all(int _fd, const uint8_t *_buf, size_t _len);
static int  recv_all(int _fd, uint8_t *_buf, size_t _len);

/* ═══════════════════════════════════════════════════════════════════════════ */
/*                          PUBLIC API                                         */
/* ═══════════════════════════════════════════════════════════════════════════ */

int net_connect(ClientContext *_ctx)
{
    if (!_ctx)
    {
        return -1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("[-] net_connect: socket creation failed");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("[-] net_connect: connect failed");
        close(sock);
        return -1;
    }

    _ctx->server_fd = sock;
    _ctx->state     = CLIENT_STATE_CONNECTED;

    return 0;
}

int net_send_recv(ClientContext *_ctx, const TLVMessage *_request,
                  TLVMessage *_response)
{
    if (!_ctx || !_request || !_response)
    {
        return -1;
    }

    if (_ctx->server_fd < 0)
    {
        fprintf(stderr, "[-] net_send_recv: not connected\n");
        return -1;
    }

    /* ── Serialize and send ── */
    uint8_t send_buf[TLV_MAX_VALUE_LEN + 2];
    uint8_t out_len = 0;

    if (serialize_msg(_request, send_buf, &out_len) != 0)
    {
        fprintf(stderr, "[-] net_send_recv: serialization failed\n");
        return -1;
    }

    if (send_all(_ctx->server_fd, send_buf, out_len) != 0)
    {
        fprintf(stderr, "[-] net_send_recv: send failed\n");
        return -1;
    }

    /* ── Receive header (2 bytes: tag + length) ── */
    uint8_t header[2];
    if (recv_all(_ctx->server_fd, header, 2) != 0)
    {
        fprintf(stderr, "[-] net_send_recv: recv header failed\n");
        return -1;
    }

    uint8_t tag    = header[0];
    uint8_t length = header[1];

    /* ── Receive value ── */
    uint8_t recv_buf[TLV_MAX_VALUE_LEN + 2];
    recv_buf[0] = tag;
    recv_buf[1] = length;

    if (length > 0)
    {
        if (recv_all(_ctx->server_fd, &recv_buf[2], length) != 0)
        {
            fprintf(stderr, "[-] net_send_recv: recv value failed\n");
            return -1;
        }
    }

    /* ── Deserialize response ── */
    if (deserialize_msg(recv_buf, (uint8_t)(2 + length), _response) != 0)
    {
        fprintf(stderr, "[-] net_send_recv: deserialization failed\n");
        return -1;
    }

    return 0;
}

void net_disconnect(ClientContext *_ctx)
{
    if (!_ctx || _ctx->server_fd < 0)
    {
        return;
    }

    close(_ctx->server_fd);
    _ctx->server_fd = -1;
    _ctx->state     = CLIENT_STATE_DISCONNECTED;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*                          STATIC HELPERS                                     */
/* ═══════════════════════════════════════════════════════════════════════════ */

/* Send exactly _len bytes — handles partial sends */
static int send_all(int _fd, const uint8_t *_buf, size_t _len)
{
    size_t total_sent = 0;

    while (total_sent < _len)
    {
        ssize_t sent = send(_fd, _buf + total_sent, _len - total_sent, 0);
        if (sent <= 0)
        {
            return -1;
        }
        total_sent += (size_t)sent;
    }

    return 0;
}

/* Receive exactly _len bytes — handles partial recvs */
static int recv_all(int _fd, uint8_t *_buf, size_t _len)
{
    size_t total_recv = 0;

    while (total_recv < _len)
    {
        ssize_t received = recv(_fd, _buf + total_recv, _len - total_recv, 0);
        if (received <= 0)
        {
            return -1;
        }
        total_recv += (size_t)received;
    }

    return 0;
}