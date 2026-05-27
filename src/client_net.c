#include "client.h"
#include <stdio.h>

int net_connect(ClientContext *_ctx)
{
    (void)_ctx;
    return 0; /* stub */
}

int net_send_recv(ClientContext *_ctx, const TLVMessage *_request, TLVMessage *_response)
{
    (void)_ctx;
    (void)_request;
    (void)_response;
    return 0; /* stub */
}

void net_disconnect(ClientContext *_ctx)
{
    (void)_ctx; /* stub */
}
