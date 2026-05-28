#ifndef __LIMITS_H__
#define __LIMITS_H__

/* ─── Network ─────────────────────────────────────── */
#define SERVER_PORT             9090
#define SERVER_IP               "127.0.0.1"
#define SERVER_BACKLOG          10
#define MAX_CLIENTS             64
#define BUFFER_SIZE             256

/* ─── Authentication ──────────────────────────────── */
#define MAX_USERNAME_LEN        32
#define MAX_PASSWORD_LEN        64

/* ─── Groups ──────────────────────────────────────── */
#define MAX_GROUPS              16
#define MAX_GROUP_NAME_LEN      32
#define MAX_MEMBERS_PER_GROUP   32

/* ─── Multicast Pool ──────────────────────────────── */
#define MULTICAST_POOL_SIZE     16
#define MULTICAST_BASE_IP       "224.0.0.1"
#define MULTICAST_BASE_PORT     5000

/* ─── IPC ─────────────────────────────────────────── */
#define IPC_MSG_KEY             0x1234
#define IPC_MSG_MAX_SIZE        64

/* ─── TLV Protocol ────────────────────────────────── */
#define TLV_MAX_VALUE_LEN       256

#endif /* __LIMITS_H__ */