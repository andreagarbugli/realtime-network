#include "rtn_socket.h"
#include "rtn_stats.h"

static void 
init_ifreq(struct ifreq *ifr, void *data, const char *ifname)
{
    memset(ifr, 0, sizeof(*ifr));
    strncpy(ifr->ifr_name, ifname, IFNAMSIZ-1);
    ifr->ifr_data = data;
}

static int
rtn_socket_check_timestamping(int sockfd, const char *ifname) 
{
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

static rtn_socket *
rtn_socket_new(const char *ifname, int port, rtn_socket_type type)
{   
    int res = 0;

    int socktype = s_rtn_socket_type_flags[type];
    fprintf(stderr, "[debug] Opening socket type %s\n", s_rtn_socket_type_str[type]);

    int sockfd = socket(AF_INET, socktype, 0);
    if (sockfd == -1) {
        perror("socket");
        goto exit_socket_error;
    }

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(port),
        .sin_addr.s_addr = INADDR_ANY,
    };

    res = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if (res < 0) {
        perror("bind");
        goto exit_cleanup;
    }

    // resuse address
    int optval = 1;
    res = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    if (res < 0) {
        perror("setsockopt");
        goto exit_cleanup;
    }

    res = setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    if (res < 0) {
        perror("setsockopt");
        goto exit_cleanup;
    }

    if (ifname) {
        struct ifreq ifr;
        init_ifreq(&ifr, NULL, ifname);
        res = setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr));
        if (res < 0) {
            perror("setsockopt");
            goto exit_cleanup;
        }
    }

    rtn_socket *sock = malloc(sizeof(rtn_socket));
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
rtn_socket_destroy(rtn_socket *sock)
{
    close(sock->fd);
    free(sock);
}

////////////////////////////////////////////////////////////////////////////////
// # Receive and Send
static int
rtn_socket_send_message(rtn_socket *sock, void *data, usize datasize, int flags)
{
    struct iovec iov = {
        .iov_base = data,
        .iov_len  = datasize,
    };
    struct msghdr msg = {
        .msg_name    = &sock->daddr,
        .msg_namelen = sock->daddr_len,
        .msg_iov     = &iov,
        .msg_iovlen  = 1,
    };

    char control[128] = {0};
    if (sock->use_txtime) {
        msg.msg_control    = control;
        msg.msg_controllen = sizeof(control);

        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type  = SCM_TXTIME;
        cmsg->cmsg_len   = CMSG_LEN(sizeof(u64));

        *((u64 *)CMSG_DATA(cmsg)) = sock->txtime;
    }

    return sendmsg(sock->fd, &msg, flags);   
}

static int
rtn_socket_receive_message(rtn_socket *sock, void *data, usize datasize, rtn_pkt_stat *pstat, int flags)
{
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    struct iovec iov = {
        .iov_base = data,
        .iov_len  = datasize,
    };
    struct msghdr mhdr = {
        .msg_name    = &addr,
        .msg_namelen = addrlen,
        .msg_iov     = &iov,
        .msg_iovlen  = 1,
    };

    char control[128] = {0};

    if (pstat) {
        mhdr.msg_control    = control;
        mhdr.msg_controllen = sizeof(control);
    }
    
    int res = recvmsg(sock->fd, &mhdr, flags);
    if (res > 0 && pstat) {        
        struct scm_timestamping *scm_ts = NULL;
        struct cmsghdr *cmsg            = NULL;
        for (cmsg = CMSG_FIRSTHDR(&mhdr); cmsg != NULL; cmsg = CMSG_NXTHDR(&mhdr, cmsg)) 
        {
            if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_TIMESTAMPING) {
                scm_ts = (struct scm_timestamping *)CMSG_DATA(cmsg);

                i64 sw = scm_ts->ts[0].tv_sec * NSEC_PER_SEC + scm_ts->ts[0].tv_nsec;
                i64 hw = scm_ts->ts[2].tv_sec * NSEC_PER_SEC + scm_ts->ts[2].tv_nsec;

                pstat->rx_tstamps.hw_ts = hw;
                pstat->rx_tstamps.sw_ts = sw;
            }
        }   
    }

    return res;
}

////////////////////////////////////////////////////////////////////////////////
// # Options
static int
rnt_socket_set_priority(rtn_socket *sock, int prio)
{
    struct sched_param sched_param = {
        .sched_priority = prio,
    };

    int res = setsockopt(sock->fd, SOL_SOCKET, SO_PRIORITY, &sched_param, sizeof(sched_param));
    if (res < 0) {
        perror("setsockopt(SO_PRIORITY)");
        return -1;
    }

    return res;
}

static int 
rtn_socket_opt_set_txtimestamp(rtn_socket *sock)
{
    struct sock_txtime sk_txtime = {
        .clockid = CLOCK_TAI,
        .flags   = 0,
    };

    int res = setsockopt(sock->fd, SOL_SOCKET, SO_TXTIME, &sk_txtime, sizeof(sk_txtime));
    if (res < 0) {
        perror("setsockopt(SO_TXTIME)");
        return -1;
    }

    sock->use_txtime = true;
    return res;
}

////////////////////////////////////////////////////////////////////////////////
// # Timestamping
static int 
rtn_socket_enable_timestamping(rtn_socket *sock, const char *ifname)
{
    int res, opt;
    int sockfd = sock->fd;

    int ts_flags = SOF_TIMESTAMPING_RX_HARDWARE     // [GF] network adapter provides hardware timestamps at receive
                 | SOF_TIMESTAMPING_RX_SOFTWARE     // [GF] RX timestamp when data enters the kernel (just after the device driver has processed it)
                 | SOF_TIMESTAMPING_TX_HARDWARE     // [GF] network adapter provides hardware timestamps at transmit
                 | SOF_TIMESTAMPING_TX_SOFTWARE     // [GF] TX timestamp when data leaves the kernel
                 | SOF_TIMESTAMPING_TX_SCHED        // [GF] before entering the packet scheduler
                 | SOF_TIMESTAMPING_SOFTWARE        // [RF] report any software timestamps
                 | SOF_TIMESTAMPING_RAW_HARDWARE    // [RF] report raw hardware timestamps
                 | SOF_TIMESTAMPING_OPT_ID          // [OF] include a unique identifier for each timestamp
                 | SOF_TIMESTAMPING_OPT_TX_SWHW;    // [OF] report both software and hardware TX timestamps

    res = setsockopt(sockfd, SOL_SOCKET, SO_TIMESTAMPING, &ts_flags, sizeof(ts_flags));
    if (res < 0) {
        perror("setsockopt");
        res = -1;
    }

    int tx_type                   = HWTSTAMP_TX_ON;
    int rx_filter                 = HWTSTAMP_FILTER_ALL;
    struct hwtstamp_config config = { .tx_type = tx_type, .rx_filter = rx_filter };

    struct ifreq ifr;
    init_ifreq(&ifr, &config, ifname);
    res = ioctl(sockfd, SIOCSHWTSTAMP, &ifr);
    if (res < 0) {
        perror("ioctl(SIOCSHWTSTAMP)");
        return -1;
    }

    opt = 1;
    res = setsockopt(sockfd, SOL_SOCKET, SO_SELECT_ERR_QUEUE, &opt, sizeof(opt));
    if (res < 0) {
        perror("setsockopt(SO_SELECT_ERR_QUEUE)");
        return -1;
    }

    // check timestamping
    socklen_t optlen = sizeof(ts_flags);
    res = getsockopt(sockfd, SOL_SOCKET, SO_TIMESTAMPING, &ts_flags, &optlen);
    if (res < 0) {
        perror("getsockopt");
        return -1;
    }

    // char printbuf[1024];
    // usize written = snprintf(printbuf, sizeof(printbuf), "Timestamping flags: ");

    // if (ts_flags & SOF_TIMESTAMPING_RX_HARDWARE)   written += snprintf(printbuf + written, sizeof(printbuf) - written, "RX_HARDWARE ");
    // if (ts_flags & SOF_TIMESTAMPING_RX_SOFTWARE)   written += snprintf(printbuf + written, sizeof(printbuf) - written, "RX_SOFTWARE ");
    // if (ts_flags & SOF_TIMESTAMPING_TX_HARDWARE)   written += snprintf(printbuf + written, sizeof(printbuf) - written, "TX_HARDWARE ");
    // if (ts_flags & SOF_TIMESTAMPING_TX_SOFTWARE)   written += snprintf(printbuf + written, sizeof(printbuf) - written, "TX_SOFTWARE ");
    // if (ts_flags & SOF_TIMESTAMPING_TX_SCHED)      written += snprintf(printbuf + written, sizeof(printbuf) - written, "TX_SCHED ");
    // if (ts_flags & SOF_TIMESTAMPING_SOFTWARE)      written += snprintf(printbuf + written, sizeof(printbuf) - written, "SOFTWARE ");
    // if (ts_flags & SOF_TIMESTAMPING_RAW_HARDWARE)  written += snprintf(printbuf + written, sizeof(printbuf) - written, "RAW_HARDWARE ");
    // if (ts_flags & SOF_TIMESTAMPING_OPT_ID)        written += snprintf(printbuf + written, sizeof(printbuf) - written, "OPT_ID ");
    // if (ts_flags & SOF_TIMESTAMPING_OPT_TX_SWHW)   written += snprintf(printbuf + written, sizeof(printbuf) - written, "OPT_TX_SWHW ");

    // printf("%s\n", printbuf);

    return res;
}

static int
rtn_socket_disable_timestamping(rtn_socket *sock)
{
    int res = 0;
    int sockfd = sock->fd;

    int ts_flags = 0;
    res = setsockopt(sockfd, SOL_SOCKET, SO_TIMESTAMPING, &ts_flags, sizeof(ts_flags));
    if (res < 0) {
        perror("setsockopt");
        res = -1;
    }

    return res;
}