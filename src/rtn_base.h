#ifndef RT_BASE_H
#define RT_BASE_H

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <linux/errqueue.h>
#include <linux/ethtool.h>
#include <linux/if_ether.h>
#include <linux/net_tstamp.h>
#include <linux/sockios.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

////////////////////////////////////////////////////////////////////////////////
// # Definitions

// ## Types
typedef int8_t      i8;
typedef int16_t     i16;
typedef int32_t     i32;
typedef int64_t     i64;
typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;

typedef float       f32;
typedef double      f64;

typedef u8          byte;   // byte
typedef u32         b32;    // boolean

typedef uintptr_t   usize;
typedef intptr_t    isize;

// ## Utility macros
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

// ## Constants
// ### Time 
#define NSER_PER_SEC 1000000000

////////////////////////////////////////////////////////////////////////////////
// # Core Functions

// ## Time
static inline i64 os_time_get_rt_ns    (void)     { struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); return ts.tv_sec * NSER_PER_SEC + ts.tv_nsec; }
static inline i64 os_time_get_ns       (void)     { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * NSER_PER_SEC + ts.tv_nsec; }
static inline i64 os_time_normalize_ts (i64 time) { return time / NSER_PER_SEC * NSER_PER_SEC; }

// ## Scheduling
typedef enum 
{
    OS_SCHED_FIFO,
    OS_SCHED_RR,
    OS_SCHED_OTHER,
} os_sched_policy;

static int s_os_sched_policy_map[] = {
    [OS_SCHED_FIFO]  = SCHED_FIFO,
    [OS_SCHED_RR]    = SCHED_RR,
    [OS_SCHED_OTHER] = SCHED_OTHER,
};

static inline int os_sched_set_policy(int policy, int prio) 
{
    struct sched_param param = { .sched_priority = prio };
    int os_policy = s_os_sched_policy_map[policy];
    return sched_setscheduler(0, os_policy, &param);
}

static inline int os_sched_set_affinity(int *cpus, usize num_cpus) 
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (usize i = 0; i < num_cpus; i++)  CPU_SET(cpus[i], &cpuset);
    return sched_setaffinity(0, sizeof(cpuset), &cpuset);
} 

// ## Memory
static inline int os_vm_lock    (void *addr, size_t len) { return mlock(addr, len); }
static inline int os_vm_lockall (void)                   { return mlockall(MCL_CURRENT | MCL_FUTURE); } 

#endif // RT_BASE_H