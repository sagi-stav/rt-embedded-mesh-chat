#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <stdint.h>
#include "limits.h"

/* ─── Message Tags ────────────────────────────────────────────────────────── */
typedef enum {
    /* Client → Server */
    MSG_REGISTER    = 1,
    MSG_LOGIN       = 2,
    MSG_LOGOUT      = 3,
    MSG_EXIT        = 4,
    MSG_JOIN_GROUP  = 5,
    MSG_LEAVE_GROUP = 6,
    MSG_CREATE_GROUP = 7,

    /* Server → Client */
    MSG_SUCCESS     = 100,
    MSG_ERROR       = 101,
    MSG_GROUP_INFO  = 102
} MessageTag;

/* ─── Error Codes ─────────────────────────────────────────────────────────── */
typedef enum {
    ERR_NONE              = 0,
    ERR_USER_EXISTS       = 1,
    ERR_USER_NOT_FOUND    = 2,
    ERR_WRONG_PASSWORD    = 3,
    ERR_ALREADY_LOGGED_IN = 4,
    ERR_GROUP_NOT_FOUND   = 5,
    ERR_GROUP_FULL        = 6,
    ERR_NOT_IN_GROUP      = 7,
    ERR_SERVER_FULL       = 8
} ErrorCode;

/* ─── TLV Message ─────────────────────────────────────────────────────────── */
typedef struct {
    uint8_t tag;                    /* MessageTag                            */
    uint8_t length;                 /* Length of value[] in bytes (max 255)  */
    uint8_t value[TLV_MAX_VALUE_LEN];
} TLVMessage;

/* ─── Public API ──────────────────────────────────────────────────────────── */

/**
 * @brief Serialize a TLVMessage into a flat byte buffer for TCP transmission.
 * @param[in]  _msg     - TLVMessage to serialize
 * @param[out] _buf     - Output buffer (must be >= 2 + _msg->length bytes)
 * @param[out] _out_len - Total bytes written to _buf
 * @return 0 on success, -1 on invalid input
 */
int serialize_msg(const TLVMessage *_msg, uint8_t *_buf, uint8_t *_out_len);

/**
 * @brief Deserialize a flat byte buffer received from TCP into a TLVMessage.
 * @param[in]  _buf - Input buffer received from TCP
 * @param[in]  _len - Number of bytes in _buf (must be >= 2)
 * @param[out] _msg - Output TLVMessage
 * @return 0 on success, -1 on malformed input
 */
int deserialize_msg(const uint8_t *_buf, uint8_t _len, TLVMessage *_msg);

/**
 * @brief Build a MSG_REGISTER or MSG_LOGIN message.
 * @param[out] _msg      - Output TLVMessage
 * @param[in]  _tag      - MSG_REGISTER or MSG_LOGIN
 * @param[in]  _username - Null-terminated username string
 * @param[in]  _password - Null-terminated password string
 * @return 0 on success, -1 on invalid input or overflow
 */
int build_auth_msg(TLVMessage *_msg, MessageTag _tag,
                   const char *_username, const char *_password);

/**
 * @brief Build a MSG_JOIN_GROUP or MSG_LEAVE_GROUP message.
 * @param[out] _msg        - Output TLVMessage
 * @param[in]  _tag        - MSG_JOIN_GROUP or MSG_LEAVE_GROUP
 * @param[in]  _group_name - Null-terminated group name string
 * @return 0 on success, -1 on invalid input or overflow
 */
int build_group_msg(TLVMessage *_msg, MessageTag _tag, const char *_group_name);

/**
 * @brief Build a simple tag-only message (MSG_LOGOUT, MSG_EXIT).
 * @param[out] _msg - Output TLVMessage
 * @param[in]  _tag - MSG_LOGOUT or MSG_EXIT
 * @return 0 on success, -1 on invalid input
 */
int build_simple_msg(TLVMessage *_msg, MessageTag _tag);

/**
 * @brief Build a MSG_SUCCESS response.
 * @param[out] _msg - Output TLVMessage
 * @return 0 on success, -1 on invalid input
 */
int build_success_msg(TLVMessage *_msg);

/**
 * @brief Build a MSG_ERROR response.
 * @param[out] _msg  - Output TLVMessage
 * @param[in]  _code - ErrorCode to send
 * @return 0 on success, -1 on invalid input
 */
int build_error_msg(TLVMessage *_msg, ErrorCode _code);

/**
 * @brief Build a MSG_GROUP_INFO response.
 * @param[out] _msg  - Output TLVMessage
 * @param[in]  _ip   - Null-terminated multicast IP string (e.g. "224.0.0.1")
 * @param[in]  _port - Multicast port in host byte order
 * @return 0 on success, -1 on invalid input or overflow
 */
int build_group_info_msg(TLVMessage *_msg, const char *_ip, uint16_t _port);

/**
 * @brief Parse a MSG_GROUP_INFO response into IP string and port.
 * @param[in]  _msg  - TLVMessage with tag MSG_GROUP_INFO
 * @param[out] _ip   - Output buffer for IP string (must be >= 16 bytes)
 * @param[out] _port - Output port in host byte order
 * @return 0 on success, -1 on malformed message
 */
int parse_group_info(const TLVMessage *_msg, char *_ip, uint16_t *_port);

#endif /* __PROTOCOL_H__ */