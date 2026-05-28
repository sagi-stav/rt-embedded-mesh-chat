#include "client.h"
#include "protocol.h"
#include "limits.h"

#include <stdio.h>
#include <string.h>

/* ─── Static Forward Declarations ────────────────────────────────────────── */
static void     screen1_print_menu(void);
static void     screen2_print_menu(void);
static void     read_input(const char *_prompt, char *_buf, size_t _max_len);
static int      handle_register(ClientContext *_ctx);
static int      handle_login(ClientContext *_ctx);
static int      handle_create_group(ClientContext *_ctx);
static int      handle_join_group(ClientContext *_ctx);
static int      handle_leave_group(ClientContext *_ctx);
static int      handle_logout(ClientContext *_ctx);
static void     print_server_error(const TLVMessage *_response);

/* ═══════════════════════════════════════════════════════════════════════════ */
/*                          PUBLIC API                                         */
/* ═══════════════════════════════════════════════════════════════════════════ */

void ui_run_screen1(ClientContext *_ctx)
{
    int choice;

    while (1)
    {
        screen1_print_menu();
        if (scanf("%d", &choice) != 1)
        {
            /* Flush bad input */
            while (getchar() != '\n');
            continue;
        }
        while (getchar() != '\n'); /* flush newline */

        switch (choice)
        {
            case 1:
                if (handle_register(_ctx) == 0)
                {
                    printf("[+] Registration successful! Please log in.\n");
                }
                break;

            case 2:
                if (handle_login(_ctx) == 0)
                {
                    printf("[+] Login successful! Welcome, %s.\n", _ctx->username);
                    ui_run_screen2(_ctx);
                }
                break;

            case 3:
                printf("[*] Goodbye!\n");
                client_exit(_ctx);
                return;

            default:
                printf("[-] Invalid option. Please try again.\n");
                break;
        }
    }
}

void ui_run_screen2(ClientContext *_ctx)
{
    int choice;

    while (1)
    {
        screen2_print_menu();
        if (scanf("%d", &choice) != 1)
        {
            while (getchar() != '\n');
            continue;
        }
        while (getchar() != '\n');

        switch (choice)
        {
            case 1:
                handle_create_group(_ctx);
                break;

            case 2:
                handle_join_group(_ctx);
                break;

            case 3:
                handle_leave_group(_ctx);
                break;

            case 4:
                if (handle_logout(_ctx) == 0)
                {
                    printf("[+] Logged out. Returning to main screen.\n");
                    return; /* Back to Screen 1 */
                }
                break;

            default:
                printf("[-] Invalid option. Please try again.\n");
                break;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*                          STATIC HELPERS                                     */
/* ═══════════════════════════════════════════════════════════════════════════ */

static void screen1_print_menu(void)
{
    printf("\n╔══════════════════════════════╗\n");
    printf("║      LAN Mesh Chat           ║\n");
    printf("╠══════════════════════════════╣\n");
    printf("║  1. Register                 ║\n");
    printf("║  2. Login                    ║\n");
    printf("║  3. Exit                     ║\n");
    printf("╚══════════════════════════════╝\n");
    printf("Choice: ");
}

static void screen2_print_menu(void)
{
    printf("\n╔══════════════════════════════╗\n");
    printf("║       Group Management       ║\n");
    printf("╠══════════════════════════════╣\n");
    printf("║  1. Create Group             ║\n");
    printf("║  2. Join Group               ║\n");
    printf("║  3. Leave Group              ║\n");
    printf("║  4. Logout                   ║\n");
    printf("╚══════════════════════════════╝\n");
    printf("Choice: ");
}

static void read_input(const char *_prompt, char *_buf, size_t _max_len)
{
    printf("%s", _prompt);
    if (fgets(_buf, (int)_max_len, stdin) != NULL)
    {
        /* Strip trailing newline */
        size_t len = strlen(_buf);
        if (len > 0 && _buf[len - 1] == '\n')
        {
            _buf[len - 1] = '\0';
        }
    }
}

static void print_server_error(const TLVMessage *_response)
{
    if (_response->tag != (uint8_t)MSG_ERROR || _response->length < 1)
    {
        printf("[-] Unknown server error.\n");
        return;
    }

    switch ((ErrorCode)_response->value[0])
    {
        case ERR_USER_EXISTS:       printf("[-] Username already taken.\n");         break;
        case ERR_USER_NOT_FOUND:    printf("[-] Username not found.\n");             break;
        case ERR_WRONG_PASSWORD:    printf("[-] Incorrect password.\n");             break;
        case ERR_ALREADY_LOGGED_IN: printf("[-] User already logged in.\n");         break;
        case ERR_GROUP_NOT_FOUND:   printf("[-] Group does not exist.\n");           break;
        case ERR_GROUP_FULL:        printf("[-] Group is full.\n");                  break;
        case ERR_NOT_IN_GROUP:      printf("[-] You are not in this group.\n");      break;
        case ERR_SERVER_FULL:       printf("[-] Server is full.\n");                 break;
        default:                    printf("[-] Unknown error code.\n");             break;
    }
}

static int handle_register(ClientContext *_ctx)
{
    char     username[MAX_USERNAME_LEN];
    char     password[MAX_PASSWORD_LEN];
    TLVMessage request, response;

    read_input("  Username: ", username, sizeof(username));
    read_input("  Password: ", password, sizeof(password));

    if (build_auth_msg(&request, MSG_REGISTER, username, password) != 0)
    {
        printf("[-] Invalid input.\n");
        return -1;
    }

    if (net_send_recv(_ctx, &request, &response) != 0)
    {
        printf("[-] Server communication failed.\n");
        return -1;
    }

    if (response.tag != (uint8_t)MSG_SUCCESS)
    {
        print_server_error(&response);
        return -1;
    }

    return 0;
}

static int handle_login(ClientContext *_ctx)
{
    char     username[MAX_USERNAME_LEN];
    char     password[MAX_PASSWORD_LEN];
    TLVMessage request, response;

    read_input("  Username: ", username, sizeof(username));
    read_input("  Password: ", password, sizeof(password));

    if (build_auth_msg(&request, MSG_LOGIN, username, password) != 0)
    {
        printf("[-] Invalid input.\n");
        return -1;
    }

    if (net_send_recv(_ctx, &request, &response) != 0)
    {
        printf("[-] Server communication failed.\n");
        return -1;
    }

    if (response.tag != (uint8_t)MSG_SUCCESS)
    {
        print_server_error(&response);
        return -1;
    }

    /* Save username in context for display */
    strncpy(_ctx->username, username, MAX_USERNAME_LEN - 1);
    _ctx->username[MAX_USERNAME_LEN - 1] = '\0';
    _ctx->state = CLIENT_STATE_LOGGED_IN;

    return 0;
}

static int handle_create_group(ClientContext *_ctx)
{
    char     group_name[MAX_GROUP_NAME_LEN];
    TLVMessage request, response;

    /* Prevent overflow on the client state array */
    if (_ctx->active_group_count >= MAX_GROUPS)
    {
        printf("[-] Maximum number of active groups reached.\n");
        return -1;
    }

    read_input("  Group name: ", group_name, sizeof(group_name));

    if (build_group_msg(&request, MSG_CREATE_GROUP, group_name) != 0)
    {
        printf("[-] Invalid group name.\n");
        return -1;
    }

    if (net_send_recv(_ctx, &request, &response) != 0)
    {
        printf("[-] Server communication failed.\n");
        return -1;
    }

    if (response.tag != (uint8_t)MSG_GROUP_INFO)
    {
        print_server_error(&response);
        return -1;
    }

    char ip[16];
    uint16_t port;
    if (parse_group_info(&response, ip, &port) != 0)
    {
        printf("[-] Malformed group info received from server.\n");
        return -1;
    }

    /* Save the new group session state */
    GroupSession *session = &_ctx->active_groups[_ctx->active_group_count];
    memset(session, 0, sizeof(GroupSession));
    strncpy(session->group_name, group_name, MAX_GROUP_NAME_LEN - 1);
    strncpy(session->multicast_ip, ip, sizeof(session->multicast_ip) - 1);
    session->multicast_port = port;
    _ctx->active_group_count++;
    client_spawn_chat_windows(_ctx, session);
    printf("[+] Group created successfully! IP: %s, Port: %u\n", ip, port);
    return 0;
}

static int handle_join_group(ClientContext *_ctx)
{
    char     group_name[MAX_GROUP_NAME_LEN];
    TLVMessage request, response;
    int i;

    if (_ctx->active_group_count >= MAX_GROUPS)
    {
        printf("[-] Maximum number of active groups reached.\n");
        return -1;
    }

    read_input("  Group name: ", group_name, sizeof(group_name));

    /* Check if already joined locally */
    for (i = 0; i < _ctx->active_group_count; i++)
    {
        if (strncmp(_ctx->active_groups[i].group_name, group_name, MAX_GROUP_NAME_LEN) == 0)
        {
            printf("[-] You are already connected to group '%s'.\n", group_name);
            return -1;
        }
    }

    if (build_group_msg(&request, MSG_JOIN_GROUP, group_name) != 0)
    {
        printf("[-] Invalid group name.\n");
        return -1;
    }

    if (net_send_recv(_ctx, &request, &response) != 0)
    {
        printf("[-] Server communication failed.\n");
        return -1;
    }

    if (response.tag != (uint8_t)MSG_GROUP_INFO)
    {
        print_server_error(&response);
        return -1;
    }

    char ip[16];
    uint16_t port;
    if (parse_group_info(&response, ip, &port) != 0)
    {
        printf("[-] Malformed group info received from server.\n");
        return -1;
    }

    /* Save the joined group session state */
    GroupSession *session = &_ctx->active_groups[_ctx->active_group_count];
    memset(session, 0, sizeof(GroupSession));
    strncpy(session->group_name, group_name, MAX_GROUP_NAME_LEN - 1);
    strncpy(session->multicast_ip, ip, sizeof(session->multicast_ip) - 1);
    session->multicast_port = port;
    _ctx->active_group_count++;

    client_spawn_chat_windows(_ctx, session);
    printf("[+] Joined group '%s' successfully! IP: %s, Port: %u\n", group_name, ip, port);
    return 0;
}

static int handle_leave_group(ClientContext *_ctx)
{
    char     group_name[MAX_GROUP_NAME_LEN];
    TLVMessage request, response;
    int target_index = -1;
    int i;

    read_input("  Group name to leave: ", group_name, sizeof(group_name));

    /* Ensure we are tracking this group locally before contacting the server */
    for (i = 0; i < _ctx->active_group_count; i++)
    {
        if (strncmp(_ctx->active_groups[i].group_name, group_name, MAX_GROUP_NAME_LEN) == 0)
        {
            target_index = i;
            break;
        }
    }

    if (target_index < 0)
    {
        printf("[-] You are not connected to group '%s' locally.\n", group_name);
        return -1;
    }

    if (build_group_msg(&request, MSG_LEAVE_GROUP, group_name) != 0)
    {
        printf("[-] Invalid group name.\n");
        return -1;
    }

    if (net_send_recv(_ctx, &request, &response) != 0)
    {
        printf("[-] Server communication failed.\n");
        return -1;
    }

    if (response.tag != (uint8_t)MSG_SUCCESS)
    {
        print_server_error(&response);
        return -1;
    }

    /* Teardown the local state (this will eventually kill the chat terminals) */
    client_close_group_session(&_ctx->active_groups[target_index]);

    /* Shift the array to remove the gap */
    for (i = target_index; i < _ctx->active_group_count - 1; i++)
    {
        _ctx->active_groups[i] = _ctx->active_groups[i + 1];
    }

    _ctx->active_group_count--;
    memset(&_ctx->active_groups[_ctx->active_group_count], 0, sizeof(GroupSession));

    printf("[+] Left group '%s'.\n", group_name);
    return 0;
}

static int handle_logout(ClientContext *_ctx)
{
    return client_logout(_ctx);
}

