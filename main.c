#define _GNU_SOURCE
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>

#include <arpa/inet.h>

#include <netinet/in.h>
#include <netinet/ip.h>

#include <net/if.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define NSER_PER_SEC 1000000000

enum {
    ROLE_TALKER   = 0,
    ROLE_LISTENER = 1,
    ROLE_PING     = 2,
    ROLE_PONG     = 3,
};

static const char *g_roleid_to_string[] = {
    [ROLE_TALKER]   = "talker",
    [ROLE_LISTENER] = "listener",
    [ROLE_PING]     = "ping",
    [ROLE_PONG]     = "pong",
};

static int 
role_name_to_id(const char *role) 
{
    for (size_t i = 0; i < ARRAY_SIZE(g_roleid_to_string); i++) {
        if (strcmp(role, g_roleid_to_string[i]) == 0) {
            return i;
        }
    }

    return -1;
}

typedef struct options options_t;
struct options {
    char    *sched_policy;
    int      sched_prio;
    char    *role_name;
    int      role_id;
    char    *interface;
    int      port;
    char    *dest_ip;
    int      packet_size;       // in bytes
    char    *cpus;              // CPU affinity: 0,1,2,3

    uint64_t cycle_time;        // cycle time in nanoseconds
	uint64_t num_packets;       // number of frames
    uint64_t throttle;          // number of packets to discard before sending

    bool     verbose;
};

enum {
    PAYLOAD_TYPE_UNKNOWN = 0,
    PAYLOAD_TYPE_DATA    = 1,
    PAYLOAD_TYPE_END     = 2,
};

// static char *g_payload_type_names[] = {
//     "UNKNOWN",
//     "DATA",
//     "END",
// };

typedef struct payload payload_t;
struct payload {
    int64_t    timestamp;
    int64_t    cycle;
    int64_t    jitter;
    uint32_t   seqno;
    uint8_t    type;
};

static inline int64_t
get_realtime_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * NSER_PER_SEC + ts.tv_nsec;
}

static inline int64_t
normalize_time(int64_t time) {
    return time / NSER_PER_SEC * NSER_PER_SEC;
}

void 
set_policy(int policy, int priority) {
    struct sched_param param;
    memset(&param, 0, sizeof(param));
    param.sched_priority = priority;
    if (sched_setscheduler(0, policy, &param) == -1) {
        perror("sched_setscheduler");
        exit(1);
    }
}

////////////////////////////////////////
// Globals

#define MAX_NUM_PACKETS 10000000

options_t options = {
    .sched_policy   = "rr",
    .sched_prio     = 98,
    .role_id        = ROLE_TALKER,
    .role_name      = "talker",
    .interface      = "eth0",
    .port           = 1234,
    .dest_ip        = "10.0.10.20",
    .packet_size    = 64,
    .cpus           = "0",
    
    .cycle_time     = 1000000,  // 1 ms
    .num_packets    = 1000,
    .throttle       = 0,

    .verbose        = false,
};

static int
init_socket(int port, const char *ifname) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(1);
    }

    // bind to device
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);
    if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, (void *)&ifr, sizeof(ifr)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    return sockfd;
}

static void
do_talker(options_t *options) 
{
    int sockfd = init_socket(options->port, options->interface);

    char packet[128];

    // send using sendmsg
    struct sockaddr_in dest_addr;
    dest_addr.sin_family      = AF_INET;
    dest_addr.sin_port        = htons(options->port);
    dest_addr.sin_addr.s_addr = inet_addr(options->dest_ip);

    struct iovec iov;
    iov.iov_base = packet;
    iov.iov_len  = sizeof(packet);
    // iov.iov_len  = options->packet_size;

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_name    = &dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov     = &iov;
    msg.msg_iovlen  = 1;

    payload_t *payload = (payload_t *)packet;
    payload->type = PAYLOAD_TYPE_DATA;

    int64_t start_time  = get_realtime_ns();
    int64_t wakeup_time = normalize_time(start_time + 2 * NSER_PER_SEC);

    fprintf(stderr, "Start time:  %ld\n", start_time);
    fprintf(stderr, "Wakeup time: %ld\n", wakeup_time);

    // // send 1 message using sendmsg and then sleep using clock_nanosleep to wait for next cycle
    int ret;
    struct timespec sleep_ts;
    bool stop = false;
    while (!stop) {
        sleep_ts.tv_sec  = wakeup_time / NSER_PER_SEC;
        sleep_ts.tv_nsec = wakeup_time % NSER_PER_SEC;

        ret = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &sleep_ts, NULL);
        if (ret == -1) {
            perror("clock_nanosleep");
            exit(1);
        }

        wakeup_time += options->cycle_time;

        if (payload->seqno == options->num_packets) {
            payload->type = PAYLOAD_TYPE_END;
            stop = true;
        }

        int64_t now         = get_realtime_ns();
        payload->timestamp  = now;
        payload->seqno     += 1;

        // printf("Sending packet %ld at %ld\n", payload->seqno, now);
        ret = sendmsg(sockfd, &msg, 0);
        if (ret == -1) {
            perror("sendmsg");
            exit(1);
        }
    }

    close(sockfd);
}

static void
do_listener(options_t *options) 
{
    uint64_t *latencies  = malloc(MAX_NUM_PACKETS * sizeof(uint64_t));
    size_t num_latencies = 0;

    int sockfd = init_socket(options->port, options->interface);

    char *packet = malloc(options->packet_size);
    if (packet == NULL) {
        perror("malloc");
        exit(1);
    }

    // recv using recvmsg
    struct sockaddr_in src_addr;
    socklen_t src_addr_len = sizeof(src_addr);

    struct iovec iov;
    iov.iov_base = packet;
    iov.iov_len  = options->packet_size;

    struct msghdr msg;
    msg.msg_name    = &src_addr;
    msg.msg_namelen = src_addr_len;
    msg.msg_iov     = &iov;
    msg.msg_iovlen  = 1;

    fprintf(stderr, "Listening on port %d\n", options->port);
    int ret;
    int stop = 0;
    while (!stop) {
        ret = recvmsg(sockfd, &msg, 0);
        if (ret == -1) {
            perror("recvmsg");
            exit(1);
        }

        payload_t *payload = (payload_t *)packet;
        if (payload->type == PAYLOAD_TYPE_END) {
            stop = 1;
        }

        int64_t now = get_realtime_ns();
        uint64_t latency = now - payload->timestamp;
        latencies[num_latencies++] = latency;
    }

    // calculate statistics
    uint64_t min_latency = UINT64_MAX;
    uint64_t max_latency = 0;
    uint64_t avg_latency = 0;
    for (size_t i = 0; i < num_latencies; i++) {
        uint64_t latency = latencies[i];
        if (latency < min_latency) {
            min_latency = latency;
        }
        if (latency > max_latency) {
            max_latency = latency;
        }
        avg_latency += latency;
    }

    avg_latency /= num_latencies;

    fprintf(stderr, "Peer disconnected, received %ld packets\n", num_latencies);
    fprintf(stderr, "Min latency: %ld\n", min_latency);
    fprintf(stderr, "Max latency: %ld\n", max_latency);
    fprintf(stderr, "Avg latency: %ld\n", avg_latency); 

    if (options->verbose) {        
        fprintf(stderr, "Saving results\n");
        printf("id, latencies\n");
        for (size_t i = 0; i < num_latencies; i++) {
            printf("%ld, %ld\n", i, latencies[i]);
        }
    }

    fprintf(stderr, "Done\n");

    close(sockfd);
}

static void
do_pong(options_t *opts) 
{
    int sockfd = init_socket(opts->port, opts->interface);

    char *packet = malloc(opts->packet_size);
    if (packet == NULL) {
        perror("malloc");
        exit(1);
    }

    // recv using recvmsg
    struct sockaddr_in src_addr;
    socklen_t src_addr_len = sizeof(src_addr);

    struct iovec iov;
    iov.iov_base = packet;
    iov.iov_len  = opts->packet_size;

    struct msghdr msg;
    msg.msg_name    = &src_addr;
    msg.msg_namelen = src_addr_len;
    msg.msg_iov     = &iov;
    msg.msg_iovlen  = 1;

    fprintf(stderr, "Listening on port %d\n", opts->port);

    int64_t cycle_time     = 0;
    int64_t last_recv_time = 0;
    int ret;
    int stop = 0;
    while (!stop) {
        ret = recvmsg(sockfd, &msg, 0);
        if (ret == -1) {
            perror("recvmsg");
            exit(1);
        }

        int64_t now        = get_realtime_ns();
        payload_t *payload = (payload_t *)packet;
        cycle_time         = payload->cycle;
        if (payload->type == PAYLOAD_TYPE_END) {
            fprintf(stderr, "Received end packet\n");
            // stop = 1;
        } 
        
        memset(packet, 'a', opts->packet_size);
        payload->timestamp = now;
        payload->type      = PAYLOAD_TYPE_DATA;
        payload->jitter    = now - last_recv_time - cycle_time;
        
        ret = sendmsg(sockfd, &msg, 0);
        if (ret == -1) {
            perror("sendmsg");
            exit(1);
        }

        last_recv_time = now;
    }

    close(sockfd);
}

static void
do_ping(options_t *opts) 
{
    int sockfd = init_socket(opts->port, opts->interface);

    char packet[128] = {0};

    // send using sendmsg
    struct sockaddr_in dest_addr = {0};
    dest_addr.sin_family      = AF_INET;
    dest_addr.sin_port        = htons(opts->port);
    dest_addr.sin_addr.s_addr = inet_addr(opts->dest_ip);

    struct iovec iov = {0};
    iov.iov_base = packet;
    iov.iov_len  = opts->packet_size;

    struct msghdr msg = {0};
    msg.msg_name    = &dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov     = &iov;
    msg.msg_iovlen  = 1;

    payload_t *payload = (payload_t *)packet;
    payload->type = PAYLOAD_TYPE_DATA;

    int64_t start_time  = get_realtime_ns();
    int64_t wakeup_time = normalize_time(start_time + 2 * NSER_PER_SEC);

    fprintf(stderr, "Start time:  %ld\n", start_time);
    fprintf(stderr, "Wakeup time: %ld\n", wakeup_time);

    int64_t *rtt_latencies    = malloc(MAX_NUM_PACKETS * sizeof(uint64_t));
    int64_t *jitter_latencies = malloc(MAX_NUM_PACKETS * sizeof(uint64_t));
    uint64_t num_latencies    = 0;
    
    int ret;
    struct timespec sleep_ts;
    bool stop = false;
    while (!stop) {
        sleep_ts.tv_sec  = wakeup_time / NSER_PER_SEC;
        sleep_ts.tv_nsec = wakeup_time % NSER_PER_SEC;

        ret = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &sleep_ts, NULL);
        if (ret == -1) {
            perror("clock_nanosleep");
            exit(1);
        }

        wakeup_time += opts->cycle_time;

        memset(packet, 0, sizeof(packet));
        memset(packet, 'a', opts->packet_size);
        int64_t now = get_realtime_ns();
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
        ret = sendmsg(sockfd, &msg, 0);
        if (ret == -1) {
            perror("sendmsg");
            exit(1);
        }

        memset(packet, 0, sizeof(packet));
        ret = recvmsg(sockfd, &msg, 0);
        if (ret == -1) {
            perror("recvmsg");
            exit(1);
        }

        if (opts->throttle > 0) {
            opts->throttle -= 1;
            continue;
        }

        rtt_latencies[num_latencies]     = get_realtime_ns() - now;
        jitter_latencies[num_latencies]  = payload->jitter;
        num_latencies                   += 1;        
    }

    // calculate statistics
    uint64_t min_rtt = UINT64_MAX;
    uint64_t max_rtt = 0;
    uint64_t avg_rtt = 0;
    for (uint64_t i = 0; i < num_latencies; i++) {
        uint64_t rtt = rtt_latencies[i];
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
    int64_t min_jitter = INT64_MAX;
    int64_t max_jitter = INT64_MIN;
    int64_t avg_jitter = 0;
    for (uint64_t i = 0; i < num_latencies; i++) {
        int64_t jitter = jitter_latencies[i];
        if (jitter < min_jitter) {
            min_jitter = jitter;
        }
        if (jitter > max_jitter) {
            max_jitter = jitter;
        }
        avg_jitter += jitter;
    }

    avg_jitter /= (int64_t)num_latencies;

    if (opts->verbose) {     
        fprintf(stderr, "Saving results\n");
        printf("id, rtt, jitter, cycle_time\n");
        for (uint64_t i = 0; i < num_latencies; i++) {
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

    close(sockfd);
}

int 
main(int argc, char *argv[]) 
{
    // Parse command line options
    int opt;
    while ((opt = getopt(argc, argv, "p:P:r:i:d:o:s:c:n:C:T:v")) != -1) {
        switch (opt) {
            case 'p':
                options.sched_policy = optarg;
                break;
            case 'P':
                options.sched_prio = atoi(optarg);
                break;
            case 'r':
                options.role_name = optarg;
                break;
            case 'i':
                options.interface = optarg;
                break;
            case 'd':
                options.dest_ip = optarg;
                break;
            case 'o':
                options.port = atoi(optarg);
                break;
            case 's':
                options.packet_size = atoi(optarg);
                break;
            case 'c':
                options.cpus = optarg;
                break;
            case 'n':
                options.num_packets = atoll(optarg);
                break;
            case 'C':
                options.cycle_time = atoll(optarg);
                break;
            case 'T':
                options.throttle = atoll(optarg);
                break;
            case 'v':
                options.verbose = true;
                break;
            default:
                fprintf(stderr, "Usage: %s [-p sched_policy] [-P sched_priority] [-r role] [-i interface] [-d dest_ip] [-o port] [-s packet_size] [-c cpus] [-n num_packets] [-C cycle_time]\n", argv[0]);
                exit(1);
        }
    }

    if (options.throttle >= options.num_packets) {
        fprintf(stderr, "Throttle value must be less than number of packets\n");
        exit(1);
    }

    // Set CPU affinity
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    char *cpus  = options.cpus;
    char *token = strtok(cpus, ",");
    while (token != NULL) {
        int cpu = atoi(token);
        CPU_SET(cpu, &cpuset);
        token = strtok(NULL, ",");
    }

    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == -1) {
        perror("sched_setaffinity");
        exit(1);
    }

    // Set scheduling policy
    int policy;
    if (strcmp(options.sched_policy, "rr") == 0) {
        policy = SCHED_RR;
    } else if (strcmp(options.sched_policy, "fifo") == 0) {
        policy = SCHED_FIFO;
    } else {
        fprintf(stderr, "Invalid scheduling policy: %s\n", options.sched_policy);
        exit(1);
    }

    set_policy(policy, options.sched_prio);

    // Lock memory
    mlockall(MCL_CURRENT | MCL_FUTURE);

    fprintf(stderr, "Role: %s\n", options.role_name);

    options.role_id = role_name_to_id(options.role_name); 
    if (options.role_id == -1) {
        fprintf(stderr, "Invalid role: %s\n", options.role_name);
        exit(1);
    }

    if (options.role_id == ROLE_TALKER) {
        do_talker(&options);
    } else if (options.role_id == ROLE_LISTENER) {
        do_listener(&options);
    } else if (options.role_id == ROLE_PING) {
        do_ping(&options);
    } else if (options.role_id == ROLE_PONG) {
        do_pong(&options);
    } 

    return 0;
}