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
        perror("[-] receiver: socket creation failed");
        return 1;
    }

    /* Allow multiple sockets to use the same port (for multiple receivers) */
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        perror("[-] receiver: setsockopt SO_REUSEADDR failed");
        close(sock);
        return 1;
    }

    /* Bind to the multicast port on any interface */
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family      = AF_INET;
    local_addr.sin_port        = htons(MULTICAST_BASE_PORT);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0)
    {
        perror("[-] receiver: bind failed");
        close(sock);
        return 1;
    }

    /* Join the multicast group */
    struct ip_mreq group;
    memset(&group, 0, sizeof(group));
    group.imr_multiaddr.s_addr = inet_addr(multicast_ip);
    group.imr_interface.s_addr = htonl(INADDR_ANY);

    if (group.imr_multiaddr.s_addr == INADDR_NONE)
    {
        fprintf(stderr, "[-] receiver: invalid multicast IP: %s\n", multicast_ip);
        close(sock);
        return 1;
    }

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &group, sizeof(group)) < 0)
    {
        perror("[-] receiver: setsockopt IP_ADD_MEMBERSHIP failed");
        close(sock);
        return 1;
    }

    printf("[receiver] Listening on %s:%d as '%s'\n",
           multicast_ip, MULTICAST_BASE_PORT, username);

    char buffer[BUFFER_SIZE];

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));

        ssize_t bytes = recvfrom(sock, buffer, sizeof(buffer) - 1,
                                 0, NULL, NULL);
        if (bytes < 0)
        {
            perror("[-] receiver: recvfrom failed");
            break;
        }

        buffer[bytes] = '\0';
        printf("%s\n", buffer);
        fflush(stdout);
    }

    /* Leave multicast group cleanly */
    setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &group, sizeof(group));
    close(sock);
    return 0;
}