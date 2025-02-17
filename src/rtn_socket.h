#ifndef RTN_SOCKET_H
#define RTN_SOCKET_H

#include "rtn_base.h"

typedef struct rtn_pkt_stat rtn_pkt_stat;

typedef enum
{
    RTN_SOCK_TYPE_UDP,
    RTN_SOCK_TYPE_TCP,
    RTN_SOCK_TYPE_RAW,
} rtn_socket_type;

static const char *s_rtn_socket_type_str[] = {
    [RTN_SOCK_TYPE_UDP] = "UDP",
    [RTN_SOCK_TYPE_TCP] = "TCP",
    [RTN_SOCK_TYPE_RAW] = "RAW",
};

static const int s_rtn_socket_type_flags[] = {
    [RTN_SOCK_TYPE_UDP] = SOCK_DGRAM,
    [RTN_SOCK_TYPE_TCP] = SOCK_STREAM,
    [RTN_SOCK_TYPE_RAW] = SOCK_RAW,
};

typedef struct rtn_socket rtn_socket;
struct rtn_socket
{
    int fd;
    int port;
    const char *ifname;

    struct sockaddr daddr;
    socklen_t       daddr_len;

    // TxTime support
    bool use_txtime;
    u64  txtime;
};

////////////////////////////////////////////////////////////////////////////////
// # Create and Destroy
static rtn_socket *rtn_socket_new     (const char *ifname, int port, rtn_socket_type type);
static void        rtn_socket_destroy (rtn_socket *sock);

////////////////////////////////////////////////////////////////////////////////
// # Other
static inline int 
rtn_socket_set_dest_addr (rtn_socket *sock, struct sockaddr *daddr, socklen_t daddr_len) 
{
    memcpy(&sock->daddr, daddr, daddr_len);
    sock->daddr_len = daddr_len;
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// # Receive and Send
static int rtn_socket_send_message    (rtn_socket *sock, void *data, usize datasize, int flags);
static int rtn_socket_receive_message (rtn_socket *sock, void *data, usize datasize, rtn_pkt_stat *pstat, int flags);

static int rtn_socket_enable_timestamping (rtn_socket *sock, const char *ifname);

static inline int rtn_socket_enable_txtime (rtn_socket *sock, bool value) { sock->use_txtime = value; }

#endif // RTN_SOCKET_H