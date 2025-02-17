////////////////////////////////////////////////////////////////////////////////
// # RT Network Application
// 
// ## Todos
// - [ ] Add the `rtn_socket` to the `rtn_options` header file.

////////////////////////////////////////////////////////////////////////////////
// # Includes
#include "rtn_base.h"
#include "rtn_client.h"
#include "rtn_log.h"
#include "rtn_options.h"
#include "rtn_packet.h"
#include "rtn_ping.h"
#include "rtn_socket.h"
#include "rtn_stats.h"
#include "rtn_server.h"
#include "rtn_txrx.h"

// # C Files
#include "rtn_socket.c"

////////////////////////////////////////////////////////////////////////////////
// # Globals

static options_t g_opts = {
    .sched_policy = "fifo",
    .sched_prio   = 80,
    .role_name    = "tx",
    .interface    = "eth0",
    .port         = 9999,
    .dest_ip      = "10.0.10.20",
    .packet_size  = 256,
    .cpus         = "1",
    .cycle_time   = 1000000,  // 1 ms
    .num_packets  = 1000,
    .verbose      = false,
    .save_file    = false,
    .log_level    = "info",
};

static char *usage_str = 
    "Usage: %s [-p sched_policy] [-P sched_priority] [-r role] [-i interface]"
    "[-d dest_ip] [-o port] [-s packet_size] [-c cpus] [-n num_packets] [-C cycle_time]\n";

////////////////////////////////////////////////////////////////////////////////
// # Main
int 
main(int argc, char *argv[]) 
{
    ////////////////////////////////////////////////////////////////////////////
    // Initialization & Command Line Parsing
    int opt;
    while ((opt = getopt(argc, argv, "p:P:r:i:d:o:s:c:n:C:l:vfha")) != -1) {
        switch (opt) {
            case 'p': g_opts.sched_policy = optarg;        break;
            case 'P': g_opts.sched_prio   = atoi(optarg);  break;
            case 'r': g_opts.role_name    = optarg;        break;
            case 'i': g_opts.interface    = optarg;        break;
            case 'd': g_opts.dest_ip      = optarg;        break;
            case 'o': g_opts.port         = atoi(optarg);  break;
            case 's': g_opts.packet_size  = atoi(optarg);  break;
            case 'c': g_opts.cpus         = optarg;        break;
            case 'n': g_opts.num_packets  = atoll(optarg); break;
            case 'C': g_opts.cycle_time   = atoll(optarg); break;
            case 'l': g_opts.log_level    = optarg;        break;
            case 'v': g_opts.verbose      = true;          break;
            case 'f': g_opts.save_file    = true;          break;
            case 'a': g_opts.rt_app_test  = true;          break;
            case 'h':
            default:
                fprintf(stderr, usage_str, argv[0]);
                exit(1);
        }
    }

    // Get OS information
    uname(&g_opts.os_info);

    // Set the log level
    int log_level = LOG_INFO;
    if (cstr_eq(g_opts.log_level, "fatal"))   log_level = LOG_FATAL;
    if (cstr_eq(g_opts.log_level, "error"))   log_level = LOG_ERROR;
    if (cstr_eq(g_opts.log_level, "warn" ))   log_level = LOG_WARN;
    if (cstr_eq(g_opts.log_level, "info" ))   log_level = LOG_INFO;
    if (cstr_eq(g_opts.log_level, "debug"))   log_level = LOG_DEBUG;
    if (cstr_eq(g_opts.log_level, "trace"))   log_level = LOG_TRACE;

    logger_set_level(log_level);

    if (g_opts.num_packets > MAX_NUM_PACKETS) {
        error("Number of packets exceeds the maximum limit: %d\n", MAX_NUM_PACKETS);
        exit(1);
    }
    
    debug("Starting RT Network Application: %s\n", g_opts.role_name);
    g_opts.role_id = role_name_to_id(g_opts.role_name); 
    if (g_opts.role_id == -1) {
        error("Invalid role: %s\n", g_opts.role_name);
        exit(1);
    }

    if (g_opts.rt_app_test && g_opts.role_id != ROLE_PONG) {
        error("Realtime application test is only for pong role\n");
        exit(1);
    }
    
    ////////////////////////////////////////////////////////////////////////////
    // Lock memory
    os_vm_lockall();

    ////////////////////////////////////////////////////////////////////////////
    // Create the socket used for the tests

    rtn_socket *sock = rtn_socket_new(g_opts.interface, g_opts.port, RTN_SOCK_TYPE_UDP);
    if (sock == NULL) {
        error("Failed to create rtn_socket\n");
        exit(1);
    }

    struct sockaddr_in dest_addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(g_opts.port),
        .sin_addr.s_addr = inet_addr(g_opts.dest_ip),
    };
    rtn_socket_set_dest_addr(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    rtn_socket_enable_timestamping(sock, g_opts.interface);

    ////////////////////////////////////////////////////////////////////////////
    // 

    g_pkt_stats.stats = malloc(MAX_NUM_PACKETS * sizeof(rtn_pkt_stat));
    if (g_pkt_stats.stats == NULL) {
        error("Failed to allocate memory for packet stats\n");
        exit(1);
    }


#define STAT_THREAD 1
#if STAT_THREAD
    pthread_t stats_thread;
    stats_thread_args stats_args = {
        .num_packets   = g_opts.num_packets,
        .sock          = sock,
        .sem_start     = &g_opts.sem_stats_start,
    };
    if (g_opts.role_id == ROLE_TX) 
    {
        // Initialize the semaphore
        os_sem_init(&g_opts.sem_stats_start, 0, 0);

        // The stats thread will read from the error queue and gather statistics
        // only for the talker role.
        // FIXME: Can we make it more generic?   
        if (pthread_create(&stats_thread, NULL, stats_thread_fn, &stats_args) != 0) {
            error("Failed to create stats thread\n");
            exit(1);
        }

        // set the thread priority low to avoid affecting the main thread
        os_thread_set_priority(stats_thread, OS_SCHED_FIFO, 1);
    }
#endif

    {
        // Set CPU affinity
        pthread_t self = os_thread_self();

        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);

        int cpus[128]   = {0};
        usize ncpus     = 0;
        char *cpus_str  = g_opts.cpus;
        char *token     = strtok(cpus_str, ",");

        while (token != NULL) {
            int cpu       = atoi(token);
            cpus[ncpus++] = cpu;
            token         = strtok(NULL, ",");
        }

        if (os_thread_set_affinity(self, cpus, ncpus) < 0) {
            error("Failed to set CPU affinity\n");
            exit(1);
        }

        // Set scheduling policy
        os_sched_policy policy_id;
        if      (cstr_eq(g_opts.sched_policy, "rr"))        policy_id = OS_SCHED_RR;
        else if (cstr_eq(g_opts.sched_policy, "fifo"))      policy_id = OS_SCHED_FIFO;
        else                                                policy_id = OS_SCHED_OTHER; 

        if (os_thread_set_priority(self, policy_id, g_opts.sched_prio) < 0) {
            error("Failed to set thread priority\n");
            exit(1);
        }
    }

    int pkt_count = 0;
    switch (g_opts.role_id) {
        case ROLE_TX:       pkt_count = do_tx(&g_opts, sock);   break;
        case ROLE_RX:     pkt_count = do_rx(&g_opts, sock);   break;
        case ROLE_PING:         pkt_count = do_ping(&g_opts, sock); break;
        case ROLE_PONG:         pkt_count = do_pong(&g_opts, sock); break;
        default:                error("Invalid role id: %d\n", g_opts.role_id); break;
    }

#if STAT_THREAD
    if (g_opts.role_id == ROLE_TX) {
        while (!g_finished_to_gather_stats)     usleep(1000 * 10);
        pthread_join(stats_thread, NULL);
    }
#endif

    options_t *opts = &g_opts;

    ////////////////////////////////////////////////////////////////////////////
    // Write results to file
    if (opts->role_id == ROLE_TX || opts->role_id == ROLE_RX) {
        info("Saving results\n");

        char *kernel_str = NULL;

        {
            // get the cmdline use to boot the kernel
            char cmdline[1024] = {0};
            FILE *file_cmdline = fopen("/proc/cmdline", "r");
            if (file_cmdline) {
                char *ret = fgets(cmdline, sizeof(cmdline), file_cmdline);
                if (ret == NULL) {
                    error("Failed to read /proc/cmdline\n");
                    exit(1);
                }
                fclose(file_cmdline);   

                // remove newline
                cmdline[strcspn(cmdline, "\n")] = 0;
            }

            char *rt  = strstr(opts->os_info.release, "rt");
            char *pro = strstr(opts->os_info.release, "realtime");
            if (rt || pro) {
                info("Detected Realtime OS: %s %s (%s)\n", opts->os_info.sysname, opts->os_info.release, cmdline);

                // check if boot cmdline contains `rcu_nocb` or `irqaffinity`
                if (strstr(cmdline, "rcu_nocb") || strstr(cmdline, "irqaffinity")) {
                    kernel_str = "rt-params";
                } else {
                    kernel_str = rt != NULL ? "rt" : "realtime";
                }

            } else {
                info("Detected Non-Realtime OS: %s %s (%s)\n", opts->os_info.sysname, opts->os_info.release, cmdline);
                kernel_str = "linux";
            }
        }

        char *output       = NULL;
        FILE *file_results = NULL;
        if (g_opts.save_file) {
            char filename[128] = {0};
            snprintf(filename, sizeof(filename), "%s_%ldus_%s.csv", opts->role_name, opts->cycle_time / 1000, kernel_str);
            file_results = fopen(filename, "w");
            output       = filename;
        } else {
            file_results = stdout;
            output       = "STDOUT";
        }

        info("Writing results to %s\n", output);

        fprintf(file_results,
                "# cfg: P=%s, p=%d, r=%s, i=%s, d=%s, o=%d, s=%d, c=%s, n=%ld, C=%ld, v=%d\n\n",
                opts->sched_policy, opts->sched_prio, opts->role_name, opts->interface, opts->dest_ip,
                opts->port, opts->packet_size, opts->cpus, opts->num_packets, opts->cycle_time,
                opts->verbose);

        if (g_opts.role_id == ROLE_TX) {
            fprintf(file_results, "id, tx_app, tx_sched, tx_sw, tx_hw\n");
            for (int i = 0; i < pkt_count; ++i) {
                rtn_pkt_stat *pstat = &g_pkt_stats.stats[i];
                fprintf(file_results, 
                        "%ld, %ld, %ld, %ld, %ld\n", 
                        pstat->id, pstat->app_tstamps.tx_ts, pstat->tx_tstamps.sched_ts, 
                        pstat->tx_tstamps.sw_ts, pstat->tx_tstamps.hw_ts);
            }
        } else {
            fprintf(file_results, "id, rx_app, rx_sw, rx_hw\n");
            for (int i = 0; i < pkt_count; i++) {
                rtn_pkt_stat *stat = &g_pkt_stats.stats[i];
                fprintf(file_results,
                        "%ld, %ld, %ld, %ld\n", 
                        stat->id, stat->app_tstamps.rx_ts, stat->rx_tstamps.sw_ts, stat->rx_tstamps.hw_ts);
            }
        }

        fclose(file_results);
    }
    
    rtn_socket_destroy(sock);

    return 0;
}
