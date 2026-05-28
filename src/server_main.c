#include "protocol.h"
#include "limits.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int main(void)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    
    /* Allow immediate port reuse after restart */
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("[-] Mock bind failed");
        return 1;
    }

    listen(server_fd, 3);
    printf("[Mock Server] Listening on port %d...\n", SERVER_PORT);

    /* Accept a single client connection */
    int client_socket = accept(server_fd, NULL, NULL);
    if (client_socket < 0)
    {
        perror("[-] Mock accept failed");
        return 1;
    }
    printf("[Mock Server] Client connected!\n");

    /* Infinite loop to handle client requests */
    while (1)
    {
        uint8_t header[2];
        if (recv(client_socket, header, 2, 0) <= 0)
        {
            printf("[Mock Server] Client disconnected.\n");
            break;
        }

        uint8_t tag = header[0];
        uint8_t len = header[1];
        uint8_t value[TLV_MAX_VALUE_LEN];

        if (len > 0)
        {
            recv(client_socket, value, len, 0);
        }

        TLVMessage resp;
        uint8_t out_buf[TLV_MAX_VALUE_LEN + 2];
        uint8_t out_len = 0;

        /* If client asks to create or join a group, return IP and Port */
        if (tag == MSG_JOIN_GROUP || tag == MSG_CREATE_GROUP)
        {
            build_group_info_msg(&resp, MULTICAST_BASE_IP, MULTICAST_BASE_PORT);
            printf("[Mock Server] Sending Group Info: %s:%d\n", 
                   MULTICAST_BASE_IP, MULTICAST_BASE_PORT);
        }
        else /* For Register, Login, Logout, Leave */
        {
            build_success_msg(&resp);
            printf("[Mock Server] Sending Success\n");
        }

        serialize_msg(&resp, out_buf, &out_len);
        send(client_socket, out_buf, out_len, 0);
    }

    close(client_socket);
    close(server_fd);
    return 0;
}