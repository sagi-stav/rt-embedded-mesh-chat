#include "server.h"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    ServerContext *ctx = server_create();
    if (!ctx) {
        fprintf(stderr, "[server] Failed to initialise server context\n");
        return EXIT_FAILURE;
    }

    server_run(ctx);      /* blocks until SIGINT / SIGTERM */
    server_destroy(ctx);
    return EXIT_SUCCESS;
}
