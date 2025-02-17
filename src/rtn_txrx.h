#ifndef RTN_TXRX_H
#define RTN_TXRX_H

#include "rtn_base.h"

#include "rtn_options.h"
#include "rtn_socket.h"
#include "rtn_stats.h"
#include "rtn_packet.h"

static int
do_tx(options_t *opts, rtn_socket *sock)
{        
    i64 start_time  = os_time_get_rt_ns();
    i64 wakeup_time = os_time_normalize_ts(start_time + 2 * NSEC_PER_SEC);

    info("TX: Start time=%ld, Wakeup time=%ld, Total packets=%ld\n", start_time, wakeup_time, opts->num_packets);

    // signal the stats thread to start
    os_sem_post(&opts->sem_stats_start);

    char *packet = malloc(opts->packet_size);

    int ret;
    u64 pkt_count = 0;
    bool stop     = false;
    while (!stop) 
    {
        memset(packet, 0, opts->packet_size);

        struct timespec sleep_ts = {
            .tv_sec  = wakeup_time / NSEC_PER_SEC,
            .tv_nsec = wakeup_time % NSEC_PER_SEC,
        };
        ret = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &sleep_ts, NULL);
        if (ret == -1) {
            perror("clock_nanosleep");
            exit(1);
        }

        i64 now = os_time_get_rt_ns();

        // Create the packet
        payload_t *payload = (payload_t *)packet;
        payload->type      = PAYLOAD_TYPE_DATA;
        payload->timestamp = now;
        payload->seqno     = pkt_count;

        // Check if this is the last packet
        if (pkt_count == opts->num_packets - 1) {
            payload->type = PAYLOAD_TYPE_END;
            stop          = true;
        }

        ret = rtn_socket_send_message(sock, packet, opts->packet_size, 0);
        if (ret == -1) {
            perror("sendmsg");
            exit(1);
        }

        // Update packet stats
        rtn_pkt_stat *pkt_stat      = &g_pkt_stats.stats[pkt_count];
        pkt_stat->id                = pkt_count;
        pkt_stat->app_tstamps.tx_ts = now;

        // Update wakeup time for next packet
        wakeup_time += opts->cycle_time;
        pkt_count   += 1;
    }

    info("TX: Sent %ld packets\n", pkt_count);

    return pkt_count - 1; // The END is not counted.
}

static int
do_rx(options_t *opts, rtn_socket *sock)
{
    info("RX: Listening for packets...\n");

    int ret;
    int stop        = 0;
    char *packet    = malloc(opts->packet_size);
    size_t num_pkts = 0;
    while (!stop) {
        rtn_pkt_stat *stat = &g_pkt_stats.stats[num_pkts];
        ret = rtn_socket_receive_message(sock, packet, opts->packet_size, stat, 0);
        if (ret == -1) {
            if (errno == EAGAIN)    continue;

            perror("recvmsg");
            exit(1);
        }

        i64 now = os_time_get_rt_ns();

        payload_t *payload = (payload_t *)packet;
        switch (payload->type) {
            case PAYLOAD_TYPE_IGNORE:   continue;
            case PAYLOAD_TYPE_END:      stop = 1; break;
            case PAYLOAD_TYPE_DATA: {   
                stat->id                 = payload->seqno;
                stat->app_tstamps.rx_ts  = now;
                num_pkts                += 1;
            } break;
        }
    }

    return num_pkts;
}

#endif // RTN_TXRX_H