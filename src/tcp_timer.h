#ifndef TCP_TIMER_H_
#define TCP_TIMER_H_

#include "stdint.h"
#include <vector>

#include "packet.h"
#include "tcp_timer_info.h"

// Necessary parameters to recompute the RTO timer for a
// connection without analyzing packets again
typedef struct {
    const Packet* packet_;
    int32_t rtt_us_; 
    uint32_t seq_acked_;
    uint32_t seq_next_;
} RttSample;

class TcpTimer {
    public:
        // Clock granularity depends on the system, for simplicity we
        // assume 1 ms here
        static const int32_t kClockGranularityUs;

        // Minimum retransmission timeout (RTO) in microseconds, for simplicity
        // we assume 200 ms here (as used by Linux)
        static const int32_t kMinRTOUs;

        // Maximum retransmission timeout (RTO) in microseconds, for simplicity
        // we assume 120 seconds here (as used by Linux)
        static const int32_t kMaxRTOUs;

        // Maximum delayed ACK timer
        static const int32_t kMaxDelayedAckUs;

        // Adjust the RTO knowing that we it is preceded by the given number of
        // RTOs
        static uint32_t AdjustRTOForBackoff(uint32_t rto, uint8_t num_rtos);

        inline std::vector<RttSample> samples() const {
            return samples_;
        }

        // Adds an RTT sample and recomputes the smoothed RTT and timeouts,
        // following RFC 6298 and the Linux kernel implementation
        void AddSample(RttSample sample);
        void AddSample(const Packet* packet,
                const uint32_t seq_acked, const uint32_t seq_next);

        // Returns the current estimate for the retransmission timeout (RTO)
        // incorporating backoffs due to consecutive RTOs
        uint32_t GetRTO(uint8_t num_rtos) const;

        // Returns the current estimate for the retransmission timeout (RTO)
        inline uint32_t GetRTO() const {
            return GetRTO(0);
        }

        // Returns the current estimate for TLP timer
        uint32_t GetTLP(bool delayed_ack) const;

    private:
        // Even though all these numbers end up being non-negative we make them
        // signed to reduce the complexity of operations; we also scale values
        // (similarly done in the Linux kernel) to make computations easier and
        // avoid rounding errors. The alpha and beta values are hidden in the
        // computations (through bit shifting) and are the same as in RFC 6298
        // (i.e. alpha = 0.125 and beta = 0.25)

        int32_t smoothed_rtt_x8_ = 0;   // Smoothed RTT (scaled by 8)
        int32_t rtt_var_x4_ = 0;        // RTT variation (scaled by 4)
        int32_t mean_dev_x4_ = 0;       // Mean deviation (scaled by 4)
        int32_t max_mean_dev_x4_ = 0;   // Maximum mean deviation
                                        // (seen during last RTT, scaled by 4)

        // Next sequence number upon which the RTT estimate is updated (unless
        // the RTT spiked up significantly before, in which case an update is
        // immediate)
        uint32_t next_seq_ = 0;

        std::vector<RttSample> samples_;
};

#endif  /* TCP_TIMER_H_ */
