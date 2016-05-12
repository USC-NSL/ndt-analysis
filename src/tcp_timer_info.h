#ifndef TCP_TIMER_INFO_H_
#define TCP_TIMER_INFO_H_

#include "stdint.h"

class Packet;

typedef struct {
    const Packet* armed_by_;
    uint64_t delay_us_;
    uint16_t backoffs_;
    bool delayed_ack_;

    uint64_t fire_us() const;
    
    void clear() {
        armed_by_ = nullptr;
        delay_us_ = 0;
        backoffs_ = 0;
        delayed_ack_ = false;
    }
} TcpTimerInfo;

#endif  /* TCP_TIMER_INFO_H_ */
