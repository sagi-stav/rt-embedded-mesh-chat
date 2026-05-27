#include "limits.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <multicast_ip> <username>\n", argv[0]);
        return 1;
    }

    const char *multicast_ip = argv[1];
    const char *username     = argv[2];

    /* Create UDP socket */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("[-] sender: socket creation failed");
        return 1;
    }

    /* Set up multicast destination address */
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family      = AF_INET;
    dest_addr.sin_port        = htons(MULTICAST_BASE_PORT);
    dest_addr.sin_addr.s_addr = inet_addr(multicast_ip);

    if (dest_addr.sin_addr.s_addr == INADDR_NONE)
    {
        fprintf(stderr, "[-] sender: invalid multicast IP: %s\n", multicast_ip);
        close(sock);
        return 1;
    }

    /* Set TTL to 1 — stay on local LAN only */
    unsigned char ttl = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0)
    {
        perror("[-] sender: setsockopt TTL failed");
        close(sock);
        return 1;
    }

    printf("[sender] Ready. Type messages (Ctrl+C to quit).\n");
    printf("[sender] Sending as '%s' to %s:%d\n",
           username, multicast_ip, MULTICAST_BASE_PORT);

    char input[BUFFER_SIZE];
    char message[BUFFER_SIZE];

    while (1)
    {
        printf("> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            break; /* EOF or error */
        }

        /* Strip trailing newline */
        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n')
        {
            input[len - 1] = '\0';
        }

        if (strlen(input) == 0)
        {
            continue; /* Skip empty messages */
        }

        /* Format: "[username]: message" */
        int msg_len = snprintf(message, sizeof(message), "[%s]: %s", username, input);
        if (msg_len < 0)
        {
            continue;
        }

        if (sendto(sock, message, (size_t)msg_len, 0,
                   (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0)
        {
            perror("[-] sender: sendto failed");
        }
    }

    close(sock);
    return 0;
}