#include "protocol.h"
#include "limits.h"

#include <string.h>   /* memcpy, strlen */
#include <stdint.h>
#include <arpa/inet.h> /* htons          */

/* ─── Static Forward Declarations ────────────────────────────────────────── */
static int is_valid_str(const char *_str, size_t _max_len);

/* ═══════════════════════════════════════════════════════════════════════════ */
/*                          PUBLIC API                                         */
/* ═══════════════════════════════════════════════════════════════════════════ */

int serialize_msg(const TLVMessage *_msg, uint8_t *_buf, uint8_t *_out_len)
{
    if (!_msg || !_buf || !_out_len)
    {
        return -1;
    }

    _buf[0] = _msg->tag;
    _buf[1] = _msg->length;
    memcpy(&_buf[2], _msg->value, _msg->length);

    *_out_len = (uint8_t)(2 + _msg->length);
    return 0;
}

int deserialize_msg(const uint8_t *_buf, uint8_t _len, TLVMessage *_msg)
{
    if (!_buf || !_msg || _len < 2)
    {
        return -1;
    }

    _msg->tag    = _buf[0];
    _msg->length = _buf[1];

    if (_msg->length > (_len - 2))
    {
        return -1;
    }

    memcpy(_msg->value, &_buf[2], _msg->length);
    return 0;
}

int build_auth_msg(TLVMessage *_msg, MessageTag _tag,
                   const char *_username, const char *_password)
{
    if (!_msg || !_username || !_password)          return -1;
    if (_tag != MSG_REGISTER && _tag != MSG_LOGIN)  return -1;
    if (!is_valid_str(_username, MAX_USERNAME_LEN)) return -1;
    if (!is_valid_str(_password, MAX_PASSWORD_LEN)) return -1;

    size_t ulen = strlen(_username);
    size_t plen = strlen(_password);

    /* Format: [ ulen (1) | username | plen (1) | password ] */
    if ((1 + ulen + 1 + plen) > TLV_MAX_VALUE_LEN) return -1;

    _msg->tag    = (uint8_t)_tag;
    _msg->length = (uint8_t)(1 + ulen + 1 + plen);

    uint8_t offset = 0;
    _msg->value[offset++] = (uint8_t)ulen;
    memcpy(&_msg->value[offset], _username, ulen);
    offset += (uint8_t)ulen;
    _msg->value[offset++] = (uint8_t)plen;
    memcpy(&_msg->value[offset], _password, plen);

    return 0;
}

int build_group_msg(TLVMessage *_msg, MessageTag _tag, const char *_group_name)
{
    if (!_msg || !_group_name) return -1;
    /* Added MSG_CREATE_GROUP to the allowed tags */
    if (_tag != MSG_JOIN_GROUP && _tag != MSG_LEAVE_GROUP && _tag != MSG_CREATE_GROUP) return -1;
    if (!is_valid_str(_group_name, MAX_GROUP_NAME_LEN)) return -1;

    size_t glen = strlen(_group_name);

    /* Format: [ glen (1) | group_name ] */
    if ((1 + glen) > TLV_MAX_VALUE_LEN) return -1;

    _msg->tag         = (uint8_t)_tag;
    _msg->length      = (uint8_t)(1 + glen);
    _msg->value[0]    = (uint8_t)glen;
    memcpy(&_msg->value[1], _group_name, glen);

    return 0;
}

int build_simple_msg(TLVMessage *_msg, MessageTag _tag)
{
    if (!_msg)                                  return -1;
    if (_tag != MSG_LOGOUT && _tag != MSG_EXIT) return -1;

    _msg->tag    = (uint8_t)_tag;
    _msg->length = 0;

    return 0;
}

int build_success_msg(TLVMessage *_msg)
{
    if (!_msg) return -1;

    _msg->tag      = (uint8_t)MSG_SUCCESS;
    _msg->length   = 1;
    _msg->value[0] = 0;

    return 0;
}

int build_error_msg(TLVMessage *_msg, ErrorCode _code)
{
    if (!_msg) return -1;

    _msg->tag      = (uint8_t)MSG_ERROR;
    _msg->length   = 1;
    _msg->value[0] = (uint8_t)_code;

    return 0;
}

int build_group_info_msg(TLVMessage *_msg, const char *_ip, uint16_t _port)
{
    if (!_msg || !_ip)              return -1;
    if (!is_valid_str(_ip, 16))     return -1;

    size_t   ip_len       = strlen(_ip);
    uint16_t port_network = htons(_port);

    /* Format: [ ip_len (1) | ip_string | port (2 bytes, network order) ] */
    if ((1 + ip_len + 2) > TLV_MAX_VALUE_LEN) return -1;

    _msg->tag    = (uint8_t)MSG_GROUP_INFO;
    _msg->length = (uint8_t)(1 + ip_len + 2);

    uint8_t offset = 0;
    _msg->value[offset++] = (uint8_t)ip_len;
    memcpy(&_msg->value[offset], _ip, ip_len);
    offset += (uint8_t)ip_len;
    memcpy(&_msg->value[offset], &port_network, 2);

    return 0;
}

int parse_group_info(const TLVMessage *_msg, char *_ip, uint16_t *_port)
{
    if (!_msg || !_ip || !_port)
    {
        return -1;
    }

    if (_msg->tag != (uint8_t)MSG_GROUP_INFO || _msg->length < 3)
    {
        return -1;
    }

    /* Format: [ ip_len (1) | ip_string | port (2 bytes, network order) ] */
    uint8_t ip_len = _msg->value[0];

    /* Validate: need ip_len + 1 (ip_len byte) + 2 (port bytes) */
    if ((uint8_t)(1 + ip_len + 2) > _msg->length || ip_len >= 16)
    {
        return -1;
    }

    memcpy(_ip, &_msg->value[1], ip_len);
    _ip[ip_len] = '\0';

    uint16_t port_network;
    memcpy(&port_network, &_msg->value[1 + ip_len], 2);
    *_port = ntohs(port_network);

    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════ */
/*                          STATIC HELPERS                                     */
/* ═══════════════════════════════════════════════════════════════════════════ */

static int is_valid_str(const char *_str, size_t _max_len)
{
    size_t len = strlen(_str);
    return (len > 0 && len < _max_len);
}