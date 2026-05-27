#include "client.h"
#include "limits.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    ClientContext ctx;

    /* Zero-initialize the entire context */
    memset(&ctx, 0, sizeof(ClientContext));
    ctx.server_fd = -1;
    ctx.state     = CLIENT_STATE_DISCONNECTED;

    if (net_connect(&ctx) != 0)
    {
        fprintf(stderr, "[-] Failed to connect to server at port %d.\n", SERVER_PORT);
        return 1;
    }

    printf("[+] Connected to server.\n");

    ui_run_screen1(&ctx);

    return 0;
}