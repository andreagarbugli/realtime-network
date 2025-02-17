#ifndef RTN_OPTIONS_H
#define RTN_OPTIONS_H

#include "rtn_base.h"

////////////////////////////////////////////////////////////////////////////////
// # Constants
#define MAX_NUM_PACKETS 10000000

////////////////////////////////////////////////////////////////////////////////
typedef enum {
    ROLE_TX   = 0,
    ROLE_RX   = 1,
    ROLE_PING = 2,
    ROLE_PONG = 3,
} role_id_t;

static const char *g_roleid_to_string[] = {
    [ROLE_TX]   = "tx",
    [ROLE_RX]   = "rx",
    [ROLE_PING] = "ping",
    [ROLE_PONG] = "pong",
};

static inline int
role_name_to_id(const char *role) 
{
    for (size_t i = 0; i < array_size(g_roleid_to_string); i++) {
        if (cstr_eq(role, g_roleid_to_string[i]))    return i;
    }

    return -1;
}

typedef struct options options_t;
struct options {
    // Scheduling
    char    *sched_policy;
    int      sched_prio;
    char    *cpus;              // CPU affinity: 0,1,2,3

    // Application Type
    char    *role_name;
    int      role_id;

    // Network
    char    *interface;
    int      port;
    char    *dest_ip;

    // Timing
    u64      cycle_time;        // cycle time in nanoseconds
    int      clock_type;        // CLOCK_TYPE_REALTIME or CLOCK_TYPE_MONOTONIC

    // Packet Generation
    int      packet_size;       // in bytes
	u64      num_packets;       // number of frames

    // OS Info
    struct utsname  os_info;

    // Debugging
    char    *log_level;
    bool     verbose;
    bool     save_file;
    bool     rt_app_test;   // only for pong 

    // Temporary
    os_sem   sem_stats_start;
};

#endif // RTN_OPTIONS_H