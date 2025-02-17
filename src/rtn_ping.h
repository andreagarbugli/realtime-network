#ifndef RTN_PING_H
#define RTN_PING_H

#include "rtn_base.h"

#include "rtn_socket.h"
#include "rtn_packet.h"
#include "rtn_stats.h"
#include "rtn_options.h"

#define MAX_PKT_TEST    2000000
#define MAX_NUM_TESTS   20

typedef struct rt_app_stats rt_app_stats_t; 
struct rt_app_stats 
{
    u64 id;
    u64 rx_tstamp;
    u64 jitter;
};

typedef struct rt_app_stats_array rt_app_stats_array_t;
struct rt_app_stats_array 
{
    rt_app_stats_t *stats;
    usize           count;
};

static rt_app_stats_array_t s_rt_app_stats[MAX_NUM_TESTS];
static int s_num_tests = -1;
static bool s_pong_stop  = false;

void 
sigint_handler(int signo)
{
    fprintf(stderr, "Received signal %d\n", signo);
    s_pong_stop = true;
}

static int
do_pong(options_t *opts, rtn_socket *sock)
{
    u8 *packet = malloc(opts->packet_size);

    int flags = 0;
    if (opts->rt_app_test) {
        signal(SIGINT, sigint_handler);

        for (int i = 0; i < MAX_NUM_TESTS; i++) {
            s_rt_app_stats[i].stats = malloc(MAX_PKT_TEST * sizeof(rt_app_stats_t));
            s_rt_app_stats[i].count = 0;
        }

        opts->clock_type = CLOCK_TYPE_MONOTONIC;
        flags            = MSG_DONTWAIT;
    }

    int ret;
    rt_app_stats_array_t *stat_array = NULL;
    i64 cycle_time     = 0;
    i64 last_recv_time = 0;
    while (!s_pong_stop) {
        memset(packet, 0, opts->packet_size);

        debug("Waiting for packet\n");
        ret = rtn_socket_receive_message(sock, packet, opts->packet_size, NULL, flags);
        if (ret == -1) {
            if (errno == EAGAIN) {
                usleep(1);
                continue;
            }
            perror("recvmsg");
            exit(1);
        }
        
        i64 now;
        if (opts->clock_type == CLOCK_TYPE_REALTIME) {
            now = os_time_get_rt_ns();
        } else {
            now = os_time_get_ns();
        }

        payload_t *payload = (payload_t *)packet;

        if (opts->rt_app_test) {
            if (payload->seqno == 0) {
                s_num_tests += 1;
                stat_array = &s_rt_app_stats[s_num_tests];
            }
            
            stat_array->stats[stat_array->count].id         = payload->seqno;
            stat_array->stats[stat_array->count].rx_tstamp  = now;
            stat_array->stats[stat_array->count].jitter     = now - last_recv_time;
            stat_array->count                              += 1;

            memset(packet, 0, ret);

        } else {
            cycle_time         = payload->cycle;
            if (payload->type == PAYLOAD_TYPE_END) {
                fprintf(stderr, "Received end packet\n");
                // stop = 1;
            } 
        
            memset(packet, 0, ret);

            payload->timestamp = now;
            payload->type      = PAYLOAD_TYPE_DATA;
            payload->jitter    = now - last_recv_time - cycle_time;        
        }

        ret = rtn_socket_send_message(sock, packet, ret, 0);
        if (ret == -1) {
            perror("sendmsg");
            exit(1);
        }

        last_recv_time = now;
    }

    // open a file for each test to write in csv format the id,rx_tstamp
    info("Saving results for %d tests\n", s_num_tests+1);
    for (int i = 0; i <= s_num_tests; i++) {
        char filename[128] = {0};
        snprintf(filename, sizeof(filename), "rt_app_test_%d.csv", i);
        FILE *file = fopen(filename, "w");
        if (file == NULL) {
            perror("fopen");
            exit(1);
        }

        info("Writing results to %s (%d/%d)\n", filename, i+1, s_num_tests+1);        
        fprintf(file, "id,rx_tstamp,jitter\n");
        for (usize j = 0; j < s_rt_app_stats[i].count; j++) {
            rt_app_stats_t *stat = &s_rt_app_stats[i].stats[j];
            fprintf(file, "%ld,%ld,%ld\n", stat->id, stat->rx_tstamp, stat->jitter);
        }

        fclose(file);

        free(s_rt_app_stats[i].stats);
    }

    return 0;
}

static int
do_ping(options_t *opts, rtn_socket *sock)
{
    u8 *packet = malloc(opts->packet_size);
    payload_t *payload = (payload_t *)packet;
    payload->type = PAYLOAD_TYPE_DATA;

    i64 start_time  = os_time_get_rt_ns();
    i64 wakeup_time = os_time_normalize_ts(start_time + 2 * NSEC_PER_SEC);

    fprintf(stderr, "Start time:  %ld\n", start_time);
    fprintf(stderr, "Wakeup time: %ld\n", wakeup_time);

    i64 *rtt_latencies    = malloc(MAX_NUM_PACKETS * sizeof(u64));
    i64 *jitter_latencies = malloc(MAX_NUM_PACKETS * sizeof(u64));
    u64 num_latencies    = 0;
    
    int ret;
    struct timespec sleep_ts;
    bool stop = false;
    while (!stop) {
        sleep_ts.tv_sec  = wakeup_time / NSEC_PER_SEC;
        sleep_ts.tv_nsec = wakeup_time % NSEC_PER_SEC;

        ret = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &sleep_ts, NULL);
        if (ret == -1) {
            perror("clock_nanosleep");
            exit(1);
        }

        wakeup_time += opts->cycle_time;

        memset(packet, 0, opts->packet_size);
        i64 now = os_time_get_rt_ns();
        if (num_latencies == opts->num_packets - 1) {
            fprintf(stderr, "Sending end packet\n");
            payload->type = PAYLOAD_TYPE_END;
            stop          = true;
        } else {
            payload->type      = PAYLOAD_TYPE_DATA;
        }

        payload->cycle     = opts->cycle_time;
        payload->timestamp = now;
        payload->seqno     = num_latencies + 1;

        // printf("Sending packet %ld at %ld\n", payload->seqno, now);
        ret = rtn_socket_send_message(sock, packet, opts->packet_size, 0);
        if (ret == -1) {
            perror("sendmsg");
            exit(1);
        }

        memset(packet, 0, opts->packet_size);
        ret = rtn_socket_receive_message(sock, packet, opts->packet_size, NULL, 0);
        if (ret == -1) {
            perror("recvmsg");
            exit(1);
        }

        rtt_latencies[num_latencies]     = os_time_get_rt_ns() - now;
        jitter_latencies[num_latencies]  = payload->jitter;
        num_latencies                   += 1;        
    }

    // calculate statistics
    u64 min_rtt = UINT64_MAX;
    u64 max_rtt = 0;
    u64 avg_rtt = 0;
    for (u64 i = 0; i < num_latencies; i++) {
        u64 rtt = rtt_latencies[i];
        if (rtt < min_rtt) {
            min_rtt = rtt;
        }
        if (rtt > max_rtt) {
            max_rtt = rtt;
        }
        avg_rtt += rtt;
    }

    avg_rtt /= num_latencies;

    // jitter can be negative
    i64 min_jitter = INT64_MAX;
    i64 max_jitter = INT64_MIN;
    i64 avg_jitter = 0;
    for (u64 i = 0; i < num_latencies; i++) {
        i64 jitter = jitter_latencies[i];
        if (jitter < min_jitter) {
            min_jitter = jitter;
        }
        if (jitter > max_jitter) {
            max_jitter = jitter;
        }
        avg_jitter += jitter;
    }

    avg_jitter /= (i64)num_latencies;

    if (opts->verbose) {     
        fprintf(stderr, "Saving results\n");
        printf("id, rtt, jitter, cycle_time\n");
        for (u64 i = 0; i < num_latencies; i++) {
            printf("%ld, %ld, %ld, %ld\n", i, rtt_latencies[i], jitter_latencies[i], opts->cycle_time);
        }
    }

    fprintf(stderr, "Peer disconnected, received %ld packets\n", num_latencies);
    fprintf(stderr, "RTT Min: %ld\n", min_rtt);
    fprintf(stderr, "RTT Max: %ld\n", max_rtt);
    fprintf(stderr, "RTT Avg: %ld\n", avg_rtt);

    fprintf(stderr, "Jitter Min: %ld\n", min_jitter);
    fprintf(stderr, "Jitter Max: %ld\n", max_jitter);
    fprintf(stderr, "Jitter Avg: %ld\n", avg_jitter);

    fprintf(stderr, "Done\n");

    return num_latencies;
}

#endif // RTN_PING_H