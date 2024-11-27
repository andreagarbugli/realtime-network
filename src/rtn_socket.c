#include "rtn_socket.h"

static int
rt_socket_check_timestamping(int sockfd, const char *ifname) {
    struct ifreq ifr;
    struct hwtstamp_config config;
    
    // Initialize ifreq structure
    memset(&ifr, 0, sizeof(ifr));
    printf("Interface name: %s\n", ifname);
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);

    // get the interface index
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) == -1) {
        perror("ioctl(SIOCGIFINDEX)");
        return -1;
    }

    int ifindex = ifr.ifr_ifindex;
    printf("Interface index: %d\n", ifindex);

    struct ethtool_ts_info ts_info;
    memset(&ts_info, 0, sizeof(ts_info));
    ts_info.cmd = ETHTOOL_GET_TS_INFO;
    ifr.ifr_data = (void *)&ts_info;
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);

    if (ioctl(sockfd, SIOCETHTOOL, &ifr) == -1) {
        perror("ioctl(SIOCETHTOOL)");
        return -1;
    }

    // Print supported timestamping modes
    printf("Timestamping capabilities for interface %s:\n", ifname);
    if (ts_info.so_timestamping & SOF_TIMESTAMPING_TX_HARDWARE)
        printf("  Hardware transmit timestamping supported\n");
    if (ts_info.so_timestamping & SOF_TIMESTAMPING_RX_HARDWARE)
        printf("  Hardware receive timestamping supported\n");
    if (ts_info.so_timestamping & SOF_TIMESTAMPING_RAW_HARDWARE)
        printf("  Raw hardware timestamping supported\n");

    if (ts_info.so_timestamping & SOF_TIMESTAMPING_TX_SOFTWARE)
        printf("  Software transmit timestamping supported\n");
    if (ts_info.so_timestamping & SOF_TIMESTAMPING_RX_SOFTWARE)
        printf("  Software receive timestamping supported\n");

    if (ts_info.phc_index >= 0)
        printf("  PHC (PTP Hardware Clock) index: %d\n", ts_info.phc_index);
    else
        printf("  No PHC available\n");

    // Check if hardware timestamping is supported
    if (!(ts_info.so_timestamping & SOF_TIMESTAMPING_TX_HARDWARE) ||
        !(ts_info.so_timestamping & SOF_TIMESTAMPING_RX_HARDWARE)) {
        fprintf(stderr, "Hardware timestamping not fully supported on %s\n", ifname);
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);
    ifr.ifr_ifindex = ifindex;
    ifr.ifr_data = (void *)&config;

    // Retrieve current hardware timestamping settings
    if (ioctl(sockfd, SIOCGHWTSTAMP, &ifr) == -1) {
        perror("ioctl(SIOCGHWTSTAMP)");
        return -1;
    }
        
    printf("Supported timestamping modes on interface %s:\n", ifname);
    switch (config.tx_type) {
        case HWTSTAMP_TX_OFF:
            printf("  HWTSTAMP_TX_OFF: Timestamping disabled on transmit\n");
            break;
        case HWTSTAMP_TX_ON:
            printf("  HWTSTAMP_TX_ON: Timestamping enabled on transmit\n");
            break;
        default:
            printf("  Unknown tx_type: %d\n", config.tx_type);
    }
    
    switch (config.rx_filter) {
        case HWTSTAMP_FILTER_NONE:
            printf("  HWTSTAMP_FILTER_NONE: No timestamping on receive\n");
            break;
        case HWTSTAMP_FILTER_ALL:
            printf("  HWTSTAMP_FILTER_ALL: Timestamping on all packets\n");
            break;
        case HWTSTAMP_FILTER_PTP_V1_L4_EVENT:
            printf("  HWTSTAMP_FILTER_PTP_V1_L4_EVENT: PTP v1, Layer 4 event packets\n");
            break;
        case HWTSTAMP_FILTER_PTP_V1_L4_SYNC:
            printf("  HWTSTAMP_FILTER_PTP_V1_L4_SYNC: PTP v1, Layer 4 sync packets\n");
            break;
        case HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ:
            printf("  HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ: PTP v1, Layer 4 delay request packets\n");
            break;
        case HWTSTAMP_FILTER_PTP_V2_L2_EVENT:
            printf("  HWTSTAMP_FILTER_PTP_V2_L2_EVENT: PTP v2, Layer 2 event packets\n");
            break;
        case HWTSTAMP_FILTER_PTP_V2_L2_SYNC:
            printf("  HWTSTAMP_FILTER_PTP_V2_L2_SYNC: PTP v2, Layer 2 sync packets\n");
            break;
        case HWTSTAMP_FILTER_PTP_V2_L2_DELAY_REQ:
            printf("  HWTSTAMP_FILTER_PTP_V2_L2_DELAY_REQ: PTP v2, Layer 2 delay request packets\n");
            break;
        case HWTSTAMP_FILTER_PTP_V2_L4_EVENT:
            printf("  HWTSTAMP_FILTER_PTP_V2_L4_EVENT: PTP v2, Layer 4 event packets\n");
            break;
        case HWTSTAMP_FILTER_PTP_V2_L4_SYNC:
            printf("  HWTSTAMP_FILTER_PTP_V2_L4_SYNC: PTP v2, Layer 4 sync packets\n");
            break;
        case HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ:
            printf("  HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ: PTP v2, Layer 4 delay request packets\n");
            break;
        case HWTSTAMP_FILTER_PTP_V2_EVENT:
            printf("  HWTSTAMP_FILTER_PTP_V2_EVENT: PTP v2 event packets\n");
            break;
        case HWTSTAMP_FILTER_PTP_V2_SYNC:
            printf("  HWTSTAMP_FILTER_PTP_V2_SYNC: PTP v2 sync packets\n");
            break;
        case HWTSTAMP_FILTER_PTP_V2_DELAY_REQ:
            printf("  HWTSTAMP_FILTER_PTP_V2_DELAY_REQ: PTP v2 delay request packets\n");
            break;
        case HWTSTAMP_FILTER_NTP_ALL:
            printf("  HWTSTAMP_FILTER_NTP_ALL: All NTP packets\n");
            break;
        default:
            printf("  Unknown rx_filter: %d\n", config.rx_filter);
    }

    return 0;
}

static int 
rt_socket_enable_timestamping(int sockfd, const char *ifname)
{
    int res = 0;
    // Enable timestamping
    //  Timestamping generation flags:
    //   - SOF_TIMESTAMPING_RX_HARDWARE: network adapter provides hardware 
    //                                   timestamps at receive
    //   - SOF_TIMESTAMPING_RX_SOFTWARE: RX timestamp when data enters the kernel 
    //                                   (just after the device driver has processed it)
    //   - SOF_TIMESTAMPING_TX_HARDWARE: network adapter provides hardware
    //                                   timestamps at transmit
    //   - SOF_TIMESTAMPING_TX_SOFTWARE: TX timestamp when data leaves the kernel.
    //   - SOF_TIMESTAMPING_TX_SCHED:    Prio entering the packet scheduler.
    //
    //  Timestamping reporting flags:
    //   - SOF_TIMESTAMPING_SOFTWARE:     report any software timestamps
    //   - SOF_TIMESTAMPING_RAW_HARDWARE: report raw hardware timestamps
    //
    //  Timestamping options:
    //   - SOF_TIMESTAMPING_OPT_ID:       include a unique identifier for each timestamp
    //   - SOF_TIMESTAMPING_OPT_TSONLY:   kernel return ts as `cmsg` alongside empty packet
    //   - SOF_TIMESTAMPING_OPT_PKTINFO:  enable SCM_TIMESTAMPING_PKTINFO control message
    //   - SOF_TIMESTAMPING_OPT_TX_SWHW:  report both software and hardware TX timestamps
    int ts_flags = SOF_TIMESTAMPING_RX_HARDWARE | SOF_TIMESTAMPING_RX_SOFTWARE         
                 | SOF_TIMESTAMPING_TX_HARDWARE | SOF_TIMESTAMPING_TX_SOFTWARE          
                 | SOF_TIMESTAMPING_TX_SCHED;   
    ts_flags |= SOF_TIMESTAMPING_SOFTWARE | SOF_TIMESTAMPING_RAW_HARDWARE;
    ts_flags |= SOF_TIMESTAMPING_OPT_ID | SOF_TIMESTAMPING_OPT_TSONLY
             | SOF_TIMESTAMPING_OPT_PKTINFO | SOF_TIMESTAMPING_OPT_TX_SWHW;
    if (setsockopt(sockfd, SOL_SOCKET, SO_TIMESTAMPING, &ts_flags, sizeof(ts_flags)) == -1) {
        perror("setsockopt");
        res = -1;
    }


    struct hwtstamp_config config = {
        .tx_type   = HWTSTAMP_TX_ON,
        .rx_filter = HWTSTAMP_FILTER_ALL,
    };
    struct ifreq ifr = {
        .ifr_data = (void *)&config,
    };
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);
    if (ioctl(sockfd, SIOCSHWTSTAMP, &ifr) == -1) {
        perror("ioctl(SIOCSHWTSTAMP)");
        return -1;
    }

    return res;
}

static rt_socket *
rt_socket_new(const char *ifname, int port, rt_socket_type type, bool ts_on)
{
    int socktype = s_rt_socket_type_flags[type];
    fprintf(stderr, "[debug] Opening socket type %s\n", s_rt_socket_type_str[type]);

    int sockfd = socket(AF_INET, socktype, 0);
    if (sockfd == -1) {
        perror("socket");
        goto exit_socket_error;
    }

    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        goto exit_cleanup;
    }

    // resuse address
    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt");
        goto exit_cleanup;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) == -1) {
        perror("setsockopt");
        goto exit_cleanup;
    }

    if (ts_on) {
        if (rt_socket_enable_timestamping(sockfd, ifname) == -1) {
            goto exit_cleanup;
        }
    }

    if (ifname) {
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);
        if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, (void *)&ifr, sizeof(ifr)) == -1) {
            perror("setsockopt");
            goto exit_cleanup;
        }
    }

    rt_socket *sock = malloc(sizeof(rt_socket));
    sock->fd     = sockfd;
    sock->port   = port;
    sock->ifname = ifname;
    return sock;

exit_cleanup:
    close(sockfd);  
exit_socket_error:
    return NULL;
}

static void 
rt_socket_destroy(rt_socket *sock)
{
    close(sock->fd);
    free(sock);
}

////////////////////////////////////////////////////////////////////////////////
// # Receive and Send
static int
rt_socket_send_message(rt_socket *sock, struct msghdr *msg, int flags)
{
    return sendmsg(sock->fd, msg, flags);   
}

static int
rt_socket_receive_message(rt_socket *sock, struct msghdr *msg, int flags)
{
    return recvmsg(sock->fd, msg, flags);
}

static int
rt_socket_errqueue_tx(rt_socket *sock)
{
    struct msghdr msg;
    struct iovec iov;
    char buffer[1024];
    char control[1024];
    struct cmsghdr *cmsg;
    // struct sock_extended_err *err;
    struct timespec *ts = NULL;

    memset(&msg, 0, sizeof(msg));
    memset(buffer, 0, sizeof(buffer));
    memset(control, 0, sizeof(control));

    // Set up message header
    iov.iov_base = buffer;
    iov.iov_len = sizeof(buffer);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);

    // Receive message from error queue
    if (recvmsg(sock->fd, &msg, MSG_ERRQUEUE) == -1) {
        perror("recvmsg");
        return -1;
    }

    // Iterate through control messages
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_TIMESTAMPING) {
            ts = (struct timespec *)CMSG_DATA(cmsg);
            printf("Hardware timestamp: %ld.%09ld seconds\n", ts[2].tv_sec, ts[2].tv_nsec);
            printf("Software timestamp: %ld.%09ld seconds\n", ts[0].tv_sec, ts[0].tv_nsec);
            printf("Raw hardware timestamp: %ld.%09ld seconds\n", ts[1].tv_sec, ts[1].tv_nsec);
        }
    }

    return 0;
}
