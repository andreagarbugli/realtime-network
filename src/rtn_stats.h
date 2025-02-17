#ifndef RTN_STATS_H
#define RTN_STATS_H

#include "rtn_base.h"
#include "rtn_socket.h"

typedef enum
{
    RTN_PKT_TS_TYPE_UNKNOWN = 0,

    RTN_PKT_TS_TYPE_TX_APP,
    RTN_PKT_TS_TYPE_TX_HW,
    RTN_PKT_TS_TYPE_TX_SW,
    RTN_PKT_TS_TYPE_TX_SCHED,

    RTN_PKT_TS_TYPE_RX_APP,
    RTN_PKT_TS_TYPE_RX_HW,
    RTN_PKT_TS_TYPE_RX_SW,

    RTN_PKT_TS_TYPE_MAX,
} rtn_pkt_ts_type;

static const char *g_pkt_ts_type_str[] = {
    [RTN_PKT_TS_TYPE_UNKNOWN] = "UNKNOWN",

    [RTN_PKT_TS_TYPE_TX_APP]   = "TX_APP",
    [RTN_PKT_TS_TYPE_TX_HW]    = "TX_HW",
    [RTN_PKT_TS_TYPE_TX_SW]    = "TX_SW",
    [RTN_PKT_TS_TYPE_TX_SCHED] = "TX_SCHED",

    [RTN_PKT_TS_TYPE_RX_APP]   = "RX_APP",
    [RTN_PKT_TS_TYPE_RX_HW]    = "RX_HW",
    [RTN_PKT_TS_TYPE_RX_SW]    = "RX_SW",
};

struct rtn_pkt_stat {
    u64 id;
    struct {
        i64     tx_ts;
        i64     rx_ts;
    } app_tstamps;

    char __pad[40] __attribute__((aligned(64)));
    
    struct {
        i64     hw_ts;
        i64     sched_ts;
        i64     sw_ts;
    } tx_tstamps;
    struct {
        i64     hw_ts;
        i64     sw_ts;
    } rx_tstamps;    
};

typedef struct rtn_pkt_stat_array rtn_pkt_stat_array;
struct rtn_pkt_stat_array {
    rtn_pkt_stat *stats;
    usize         num_stats;
};

static rtn_pkt_stat_array g_pkt_stats                = {0};
static bool               g_finished_to_gather_stats = false;

typedef struct stats_thread_args stats_thread_args;
struct stats_thread_args {
    uint                num_packets;
    uint                num_throttles;
    rtn_socket         *sock;
    os_sem             *sem_start;
};

static void 
parse_cmsg_timestamps(struct msghdr *msg, rtn_pkt_stat *pkt_stat, uint *out_ts_id)
    // uint *out_ts_type, uint *out_snd_count)
{
    uint ts_type = 0;
    i64 sw = 0, hw = 0;
    struct scm_timestamping *scm_ts = NULL;
    struct sock_extended_err *serr  = NULL;
    struct cmsghdr *cmsg            = NULL;

    char printbuf[1024];
    usize written = snprintf(printbuf, sizeof(printbuf), "CMSG: ");

    for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL; cmsg = CMSG_NXTHDR(msg, cmsg)) 
    {   
        int cmsg_level = cmsg->cmsg_level;
        int cmsg_type  = cmsg->cmsg_type;

        written += snprintf(printbuf + written, sizeof(printbuf) - written,
                            "cmsg_level: %s, cmsg_type: %s\n", 
                            cmsg_level == SOL_SOCKET ? "SOL_SOCKET" : "SOL_IP", 
                            cmsg_type  == SO_TIMESTAMPING ? "SO_TIMESTAMPING" : "IP_RECVERR");

        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_TIMESTAMPING) 
        {
            scm_ts = (struct scm_timestamping *)CMSG_DATA(cmsg);
            sw = scm_ts->ts[0].tv_sec * NSEC_PER_SEC + scm_ts->ts[0].tv_nsec;
            hw = scm_ts->ts[2].tv_sec * NSEC_PER_SEC + scm_ts->ts[2].tv_nsec;      
        }
        else if ((cmsg_level == SOL_IP     && cmsg_type == IP_RECVERR) ||
                (cmsg_type   == SOL_PACKET && cmsg_type == PACKET_TX_TIMESTAMP)) 
        {
            serr = (struct sock_extended_err *)CMSG_DATA(cmsg);
            if (serr && serr->ee_origin == SO_EE_ORIGIN_TIMESTAMPING) {
                *out_ts_id = serr->ee_data;
                ts_type    = serr->ee_info;
            }                
        }
    }

    rtn_pkt_ts_type pkt_ts_type = RTN_PKT_TS_TYPE_UNKNOWN;
    if      (sw == 0)                      pkt_ts_type = RTN_PKT_TS_TYPE_TX_HW;
    else if (ts_type == SCM_TSTAMP_SND)    pkt_ts_type = RTN_PKT_TS_TYPE_TX_SW;
    else if (ts_type == SCM_TSTAMP_SCHED)  pkt_ts_type = RTN_PKT_TS_TYPE_TX_SCHED;

    snprintf(printbuf + written, sizeof(printbuf) - written, "\tsw: %ld, hw: %ld, id: %d, ts_type: %s\n", 
             sw, hw, *out_ts_id, g_pkt_ts_type_str[pkt_ts_type]);

    // printf("%s\n", printbuf);

    switch (pkt_ts_type) {
        case RTN_PKT_TS_TYPE_TX_SCHED:  pkt_stat->tx_tstamps.sched_ts = sw; break;
        case RTN_PKT_TS_TYPE_TX_SW:     pkt_stat->tx_tstamps.sw_ts    = sw; break;
        case RTN_PKT_TS_TYPE_TX_HW:     pkt_stat->tx_tstamps.hw_ts    = hw; break;
        default:                        printf("Unknown pkt_ts_type\n"); break;
    }
}

static void *
stats_thread_fn(void *arg)
{
    stats_thread_args *args = (stats_thread_args *)arg;

    info("Starting stats thread, expecting %d packets\n", args->num_packets);

    rtn_pkt_stat_array *pkt_stats_array = &g_pkt_stats;

    debug("Waiting for the start signal...\n");
    os_sem_wait(args->sem_start);

    int retry             = 10;
    uint ts_id            = 0;
    uint idx              = 0;
    rtn_pkt_stat tmp_stat = {0};
    while ((idx < args->num_packets - 1) || retry > 0)
    {
        char buffer[1024]  = {0};
        char control[1024] = {0};
        struct iovec iov   = { .iov_base = buffer, .iov_len = sizeof(buffer) };
        struct msghdr msg  = { .msg_iov = &iov, .msg_iovlen = 1, .msg_control = control, .msg_controllen = sizeof(control) };

        // Receive message from error queue
        int res = recvmsg(args->sock->fd, &msg, MSG_ERRQUEUE);
        if (res < 0) {
            if (errno == EAGAIN) {
                if (idx >= args->num_packets - 1)  {
                    debug("Received all packets, check if there are any more packets in the queue (retry=%d)\n", retry);   
                    retry -= 1;
                }

                usleep(1000 * 10); 
                continue; 
            }   

            error("recvmsg: %s\n", strerror(errno));         
        }

        parse_cmsg_timestamps(&msg, &tmp_stat, &ts_id);

        idx                                    = ts_id;
        pkt_stats_array->stats[idx].tx_tstamps = tmp_stat.tx_tstamps;
    }

    g_finished_to_gather_stats = true;
    
    pthread_exit(NULL);
}

#endif // RTN_STATS_H