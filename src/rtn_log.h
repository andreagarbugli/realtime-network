#ifndef RTN_LOG_H
#define RTN_LOG_H

#include "rtn_base.h"

typedef int (*log_write_op)(const char *msg, usize len);

static inline int 
__logger_default_write_op(const char *msg, usize len) 
{ 
    UNUSED(len); 
    return fprintf(stderr, "%s", msg); 
}

typedef enum {
    LOG_NONE,
    LOG_FATAL,
    LOG_ERROR,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG,
    LOG_TRACE,
    LOG_LEVEL_COUNT  // Total number of log levels
} log_level_t;

typedef struct log_level_fmt log_level_fmt;
struct log_level_fmt {
    char *level_str;   // Log level as a string
    char *color_str;   // Color string for the log level
};

static const log_level_fmt __drt_log_level_formats[LOG_LEVEL_COUNT] = {
    [LOG_NONE]  = {"NONE",  ""},
    [LOG_FATAL] = {"FATAL", ANSI_COLOR_RED},
    [LOG_ERROR] = {"ERROR", ANSI_COLOR_RED},
    [LOG_WARN]  = {"WARN",  ANSI_COLOR_YELLOW},
    [LOG_INFO]  = {"INFO",  ANSI_COLOR_GREEN},
    [LOG_DEBUG] = {"DEBUG", ANSI_COLOR_CYAN},
    [LOG_TRACE] = {"TRACE", ANSI_COLOR_MAGENTA},
};

static u32 logger_log_levelstr_to_enum(char *str)
{
    if (cstr_eq(str, "fatal"))   return LOG_FATAL;
    if (cstr_eq(str, "error"))   return LOG_ERROR;
    if (cstr_eq(str, "warn" ))   return LOG_WARN;
    if (cstr_eq(str, "info" ))   return LOG_INFO;
    if (cstr_eq(str, "debug"))   return LOG_DEBUG;
    if (cstr_eq(str, "trace"))   return LOG_TRACE;

    return LOG_NONE;
}

typedef struct logger logger;
struct logger
{
    log_level_t      level;
    bool             ts_on;
    bool             ctx_info_on; // line and file info
    log_write_op     write_op;
    char            *title;
};

static logger g_rtn_logger = {
    .level       = LOG_INFO,
    .ts_on       = false,
    .ctx_info_on = false,
    .write_op    = __logger_default_write_op,
    .title       = "rtn",
};

static inline void logger_log_set_level    (logger *log, log_level_t level)     { log->level = level; }
static inline void logger_log_set_ts       (logger *log, bool enable_ts)        { log->ts_on = enable_ts; }
static inline void logger_log_set_write_op (logger *log, log_write_op write_op) { log->write_op = write_op; }
static inline void logger_log_set_title    (logger *log, char *title)           { log->title = title; }
static inline void logger_log_set_ctx      (logger *log, bool enable_ctx)       { log->ctx_info_on = enable_ctx; }

int
__logger_log(u32 level, const char *sub, const char *fmt, ...)
{
    if (level > g_rtn_logger.level || g_rtn_logger.write_op == NULL) return 0;

    char buf[4096] = {0};
    size_t max_len = sizeof(buf);
    size_t n       = 0;

    n += snprintf(buf, max_len, "[");

    if (g_rtn_logger.title)     n += snprintf(buf+n, max_len-n, "%s ", g_rtn_logger.title);

    if (g_rtn_logger.ts_on) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        struct tm *tm = localtime(&ts.tv_sec);

        n += strftime(buf+n, max_len-n, "%Y-%m-%d %H:%M:%S", tm);
        n += snprintf(buf+n, max_len-n, ".%09ld ", ts.tv_nsec);
    }

    log_level_fmt lvl_fmt = __drt_log_level_formats[level];
    if (sub) n += snprintf(buf+n, max_len-n, "%s%5s%s %s", lvl_fmt.color_str, lvl_fmt.level_str, ANSI_COLOR_RESET, sub);
    else     n += snprintf(buf+n, max_len-n, "%s%5s%s", lvl_fmt.color_str, lvl_fmt.level_str, ANSI_COLOR_RESET);

    n += snprintf(buf+n, max_len-n, "] ");

    va_list args;
    va_start(args, fmt);
    n += vsnprintf(buf+n, max_len-n, fmt, args);
    va_end(args);

    return g_rtn_logger.write_op(buf, n);
}

#define logger_set_level(level)         logger_log_set_level(&g_rtn_logger, level)
#define logger_set_ts(enable_ts)        logger_log_set_ts(&g_rtn_logger, enable_ts)
#define logger_set_write_fn(write_fn)   logger_log_set_write_fn(&g_rtn_logger, write_fn)
#define logger_set_title(title)         logger_log_set_title(&g_rtn_logger, title)
#define logger_set_ctx(enable)          logger_log_set_ctx(&g_rtn_logger, enable)

#define debug(...)                  __logger_log(LOG_DEBUG, NULL, __VA_ARGS__)
#define info(...)                   __logger_log(LOG_INFO, NULL, __VA_ARGS__)
#define warn(...)                   __logger_log(LOG_WARN, NULL,  __VA_ARGS__)
#define error(...)                  __logger_log(LOG_ERROR, NULL, __VA_ARGS__)

// #define debug_ctx(fmt, ...)         __logger_log(LOG_DEBUG, NULL, "(%s:%d)" # fmt, __FILE__, __LINE__, ##__VA_ARGS__)

// #define debug_sys(sys, fmt, ...)     __logger_log(LOG_DEBUG, sys, fmt, ##__VA_ARGS__)
// #define info_sys(sys, fmt, ...)      __logger_log(LOG_INFO,  sys, fmt, ##__VA_ARGS__)
// #define warn_sys(sys, fmt, ...)      __logger_log(LOG_WARN,  sys, fmt, ##__VA_ARGS__)
// #define error_sys(sys, fmt, ...)     __logger_log(LOG_ERROR, sys, fmt, ##__VA_ARGS__)

#endif // RTN_LOG_H