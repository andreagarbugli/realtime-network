#ifndef RT_SOCKET_H
#define RT_SOCKET_H

#include "rtn_base.h"

typedef enum
{
    RT_SOCKET_TYPE_UDP,
    RT_SOCKET_TYPE_TCP,
    RT_SOCKET_TYPE_RAW,
} rt_socket_type;

static const char *s_rt_socket_type_str[] = {
    [RT_SOCKET_TYPE_UDP] = "UDP",
    [RT_SOCKET_TYPE_TCP] = "TCP",
    [RT_SOCKET_TYPE_RAW] = "RAW",
};

static const int s_rt_socket_type_flags[] = {
    [RT_SOCKET_TYPE_UDP] = SOCK_DGRAM,
    [RT_SOCKET_TYPE_TCP] = SOCK_STREAM,
    [RT_SOCKET_TYPE_RAW] = SOCK_RAW,
};

typedef struct rt_socket rt_socket;
struct rt_socket
{
    int fd;
    int port;
    const char *ifname;
};

////////////////////////////////////////////////////////////////////////////////
// # Create and Destroy
static rt_socket *rt_socket_new     (const char *ifname, int port, rt_socket_type type, bool ts_on);
static void       rt_socket_destroy (rt_socket *sock);

////////////////////////////////////////////////////////////////////////////////
// # Receive and Send
static int        rt_socket_send_message    (rt_socket *sock, struct msghdr *msg, int flags);
static int        rt_socket_receive_message (rt_socket *sock, struct msghdr *msg, int flags);


static int        rt_socket_errqueue_tx (rt_socket *sock);

#endif // RT_SOCKET_H