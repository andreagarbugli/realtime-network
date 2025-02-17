#ifndef RTN_BASE_H
#define RTN_BASE_H

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <netdb.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
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
#include <linux/if_packet.h>
#include <linux/net_tstamp.h>
#include <linux/sockios.h>

#include <net/ethernet.h> 
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>

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
#define VA_ARGS(...)    , ##__VA_ARGS__
#define UNUSED(x)       (void)(x)
#define array_size(x)   (sizeof(x) / sizeof((x)[0]))

// ## Constants
// ### Time 
#define NSEC_PER_SEC    1000000000

// ## String 
#define cstr_eq(s1, s2)  (strcmp(s1, s2) == 0)

// ## ASCII Colors for Terminal
#define ANSI_COLOR_RED          "\x1b[31m"
#define ANSI_COLOR_GREEN        "\x1b[32m"
#define ANSI_COLOR_YELLOW       "\x1b[33m"
#define ANSI_COLOR_BLUE         "\x1b[34m"
#define ANSI_COLOR_MAGENTA      "\x1b[35m"
#define ANSI_COLOR_CYAN         "\x1b[36m"
#define ANSI_COLOR_RESET        "\x1b[0m"

////////////////////////////////////////////////////////////////////////////////
// # Core Functions

// ## Time
typedef enum {
    CLOCK_TYPE_REALTIME,
    CLOCK_TYPE_MONOTONIC,
} clock_type;

static inline i64 os_time_get_rt_ns    (void)     { struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); return ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec; }
static inline i64 os_time_get_ns       (void)     { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec; }
static inline i64 os_time_normalize_ts (i64 time) { return time / NSEC_PER_SEC * NSEC_PER_SEC; }

// ## Thread

static inline pthread_t os_thread_self(void)                                   { return pthread_self(); }
static inline void      os_thread_set_name(pthread_t thread, const char *name) { pthread_setname_np(thread, name); }

// ### Mutex
typedef pthread_mutex_t os_mutex;

static inline int os_mutex_init   (os_mutex *mutex) { return pthread_mutex_init(mutex, NULL); }
static inline int os_mutex_lock   (os_mutex *mutex) { return pthread_mutex_lock(mutex); }
static inline int os_mutex_unlock (os_mutex *mutex) { return pthread_mutex_unlock(mutex); }

// ### Condition Variable
typedef pthread_cond_t os_cond;

static inline int os_cond_init      (os_cond *cond) { return pthread_cond_init(cond, NULL); }
static inline int os_cond_signal    (os_cond *cond) { return pthread_cond_signal(cond); }
static inline int os_cond_broadcast (os_cond *cond) { return pthread_cond_broadcast(cond); }

// ### Semaphore
typedef sem_t os_sem;

static inline int os_sem_init    (os_sem *sem, int pshared, unsigned int value) { return sem_init(sem, pshared, value); }
static inline int os_sem_wait    (os_sem *sem)                                  { return sem_wait(sem); }
static inline int os_sem_post    (os_sem *sem)                                  { return sem_post(sem); }
static inline int os_sem_destroy (os_sem *sem)                                  { return sem_destroy(sem); }

// ## Scheduling
// ### Scheduling Policy
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

static inline int 
os_sched_set_policy(int policy, int prio) 
{
    struct sched_param param = { .sched_priority = prio };
    int os_policy = s_os_sched_policy_map[policy];
    return sched_setscheduler(0, os_policy, &param);
}

static inline int 
os_sched_set_affinity(int *cpus, usize num_cpus) 
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (usize i = 0; i < num_cpus; i++)  CPU_SET(cpus[i], &cpuset);
    return sched_setaffinity(0, sizeof(cpuset), &cpuset);
} 

static inline int
os_thread_set_priority(pthread_t thread, int policy, int prio)
{
    struct sched_param param = { .sched_priority = prio };
    int os_policy = s_os_sched_policy_map[policy];
    return pthread_setschedparam(thread, os_policy, &param);
}

static inline int
os_thread_set_affinity(pthread_t thread, int *cpus, usize num_cpus) 
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (usize i = 0; i < num_cpus; i++)  CPU_SET(cpus[i], &cpuset);
    return pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);
}

// ## Memory
static inline int os_vm_lock    (void *addr, size_t len) { return mlock(addr, len); }
static inline int os_vm_lockall (void)                   { return mlockall(MCL_CURRENT | MCL_FUTURE); } 

#endif // RTN_BASE_H