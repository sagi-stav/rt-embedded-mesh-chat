#ifndef SERVER_H
#define SERVER_H

#include "protocol.h"
#include "limits.h"
#include "HashMap.h"
#include "genqueue.h"
#include "gen_dlist.h"

#include <stdint.h>

/**
 * @file server.h
 * @brief Server-side data structures and API for the LAN chat application.
 *
 * @details The server manages user registration/login and group membership
 * over TCP using a TLV protocol.  Chat messages are sent directly between
 * clients over UDP multicast — the server is never involved in delivery.
 *
 * Data structures used:
 *   - HashMap  : users  (username  -> UserRecord*)
 *   - HashMap  : groups (groupname -> GroupRecord*)
 *   - Queue    : pool of free multicast addresses (McastEntry*)
 *   - List     : per-group member tracking (socket_fd values)
 */


/* ══════════════════════════════════════════════════════════════════════════
 *  Data Structures
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief One free multicast address in the pool.
 *
 * @details Popped from mcast_queue when a group is created.
 *          Pushed back when a group closes (member_count reaches 0).
 */
typedef struct {
    char     ip[16];    /**< Multicast IP string, e.g. "224.0.0.3" */
    uint16_t port;      /**< Multicast port in host byte order      */
} McastEntry;


/**
 * @brief Per-user record stored in the user HashMap.
 *
 * @details Keyed by the username field (pointer stays stable — heap allocated).
 *          socket_fd is set on login and cleared (-1) on logout or disconnect.
 */
typedef struct {
    char   username[MAX_USERNAME_LEN]; /**< Unique username (the HashMap key)          */
    size_t password_hash;              /**< djb2 hash of password — never stored plain */
    int    logged_in;                  /**< 1 = authenticated, 0 = not                 */
    int    socket_fd;                  /**< Active TCP fd, -1 if not connected          */
} UserRecord;


/**
 * @brief Per-group record stored in the group HashMap.
 *
 * @details Keyed by the name field.
 *          members is a List of socket_fd values stored as (void*)(intptr_t)fd
 *          so no extra heap allocation is needed per member.
 */
typedef struct {
    char     name[MAX_GROUP_NAME_LEN]; /**< Unique group name (the HashMap key)    */
    char     mcast_ip[16];             /**< Multicast IP assigned to this group    */
    uint16_t mcast_port;               /**< Multicast port in host byte order      */
    List    *members;                  /**< List of (void*)(intptr_t)socket_fd     */
    int      member_count;             /**< Current number of members              */
} GroupRecord;


/**
 * @brief Global server context — one instance for the lifetime of the process.
 *
 * @details Holds every resource the server owns: the listening socket,
 *          all active client fds (watched by select()), and the three
 *          data structures for users, groups, and the free multicast pool.
 */
typedef struct {
    int      listen_fd;               /**< TCP listening socket                    */
    int      client_fds[MAX_CLIENTS]; /**< Active client fds; -1 means free slot   */
    int      running;                 /**< Main-loop flag: set to 0 to shut down   */

    HashMap *user_map;    /**< username  (char*) -> UserRecord*   */
    HashMap *group_map;   /**< groupname (char*) -> GroupRecord*  */
    Queue   *mcast_queue; /**< Free McastEntry* items             */
} ServerContext;


/* ══════════════════════════════════════════════════════════════════════════
 *  server_net.c  —  socket setup, select() loop, raw send / recv
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Allocate and initialise a ServerContext.
 *
 * @details Creates the two HashMaps, the multicast Queue, and pre-loads
 *          MULTICAST_POOL_SIZE addresses starting from MULTICAST_BASE_IP.
 *          All client_fds slots are set to -1 (free).
 *
 * @return Pointer to a new ServerContext, or NULL on allocation failure.
 */
ServerContext *server_create(void);

/**
 * @brief Bind the server port, start listening, and run the select() loop.
 *
 * @details Blocks until running is set to 0 (e.g. by a SIGINT handler).
 *          On each iteration:
 *            - accept() new connections into a free client_fds slot,
 *            - call net_read_msg() + dispatch_message() for each ready fd,
 *            - call client_disconnected() + close() on read error (EOF).
 *
 * @param[in] _ctx - Initialised ServerContext returned by server_create().
 */
void server_run(ServerContext *_ctx);

/**
 * @brief Free all resources owned by the server context.
 *
 * @details Closes every open fd, destroys the HashMaps (freeing all records),
 *          destroys the Queue (freeing all McastEntry items), and frees _ctx.
 *
 * @param[in] _ctx - ServerContext to destroy. *_ctx must not be used afterwards.
 */
void server_destroy(ServerContext *_ctx);

/**
 * @brief Read one complete TLV message from a TCP socket.
 *
 * @details Reads the 2-byte header (tag + length) then exactly length
 *          value bytes.  Loops on short reads.
 *
 * @param[in]  _fd  - Connected TCP socket fd.
 * @param[out] _msg - Output TLVMessage to fill.
 * @return 0 on success, -1 on error or connection closed.
 */
int net_read_msg(int _fd, TLVMessage *_msg);

/**
 * @brief Serialise and write one TLV message to a TCP socket.
 *
 * @details Calls serialize_msg() then loops on write() until all bytes are sent.
 *
 * @param[in] _fd  - Connected TCP socket fd.
 * @param[in] _msg - TLVMessage to send.
 * @return 0 on success, -1 on error.
 */
int net_send_msg(int _fd, const TLVMessage *_msg);


/* ══════════════════════════════════════════════════════════════════════════
 *  server_mng.c  —  message dispatch and business logic
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Route an incoming TLV message to the correct handler.
 *
 * @details Handlers implemented:
 *            MSG_REGISTER   -> register a new user
 *            MSG_LOGIN      -> authenticate an existing user
 *            MSG_LOGOUT     -> log out (stay connected for re-login)
 *            MSG_EXIT       -> log out and signal caller to close the fd
 *            MSG_JOIN_GROUP -> join (or create-then-join) a multicast group
 *            MSG_LEAVE_GROUP-> leave a group; close it if now empty
 *
 * @param[in] _ctx - Server context.
 * @param[in] _fd  - Socket fd the message arrived on.
 * @param[in] _msg - The received TLVMessage.
 * @return 0 to keep the connection open.
 * @return -1 when MSG_EXIT is received — caller must close _fd.
 */
int dispatch_message(ServerContext *_ctx, int _fd, const TLVMessage *_msg);

/**
 * @brief Clean up after a client fd closes unexpectedly.
 *
 * @details Scans every group and removes _fd from its member list.
 *          Decrements member_count for each group the client was in.
 *          Finds the matching UserRecord (by socket_fd) and marks it inactive.
 *
 * @param[in] _ctx - Server context.
 * @param[in] _fd  - The fd that just closed.
 */
void client_disconnected(ServerContext *_ctx, int _fd);


#endif /* SERVER_H */
