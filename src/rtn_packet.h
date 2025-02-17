#ifndef RTN_PACKET_H
#define RTN_PACKET_H

#include "rtn_base.h"

typedef enum {
    PAYLOAD_TYPE_UNKNOWN = 0,
    PAYLOAD_TYPE_DATA    = 1,
    PAYLOAD_TYPE_END     = 2,
    PAYLOAD_TYPE_IGNORE  = 3,
} payload_type_t;

// static char *g_payload_type_names[] = {
//     "UNKNOWN",
//     "DATA",
//     "END",
// };

typedef struct payload payload_t;
struct payload 
{
    u64     seqno;
    i64     timestamp;
    i64     cycle;
    i64     jitter;
    u8      type;
};

#endif  // RTN_PACKET_H