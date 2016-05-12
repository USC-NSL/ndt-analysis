#include "tcp_timer.h"

#include <complex>

#include "tcp_packet.h"
#include "util.h"

constexpr int32_t TcpTimer::kClockGranularityUs = 1000;
constexpr int32_t TcpTimer::kMinRTOUs = 200E3;  // 200ms
constexpr int32_t TcpTimer::kMaxRTOUs = 120E6;  // 120s
constexpr int32_t TcpTimer::kMaxDelayedAckUs = 200E3;  // 200ms

uint64_t TcpTimerInfo::fire_us() const {
    if (armed_by_ != nullptr) {
        return armed_by_->timestamp_us() + delay_us_;
    } else {
        return 0;
    }
}

void TcpTimer::AddSample(const Packet* packet,
        const uint32_t seq_acked, const uint32_t seq_next) {
    RttSample sample = {
        packet,
        static_cast<int32_t>(packet->tcp()->ack_delay_us()),
        seq_acked,
        seq_next
    };
    AddSample(sample);
}

void TcpTimer::AddSample(RttSample sample) {
    samples_.push_back(sample);

    const int32_t rtt_us = sample.rtt_us_;
    if (!smoothed_rtt_x8_) {
        // This is the first sample
        smoothed_rtt_x8_ = rtt_us << 3;
        mean_dev_x4_ = rtt_us << 1;
        rtt_var_x4_ = std::max(mean_dev_x4_, kMinRTOUs);
        max_mean_dev_x4_ = rtt_var_x4_;
        next_seq_ = sample.seq_next_;
        return;
    }

    // RFC 6298 formula for the smoothed RTT update:
    // SRTT <- 7/8 * SRTT + 1/8 R'
    //      <- SRTT + 1/8 * (R' - SRTT)
    // where R' is the new RTT measurement
    const int32_t rtt_error = rtt_us - (smoothed_rtt_x8_ >> 3);
    smoothed_rtt_x8_ += rtt_error;

    // For the rtt variation update:
    // RTTVAR <- 3/4 * RTTVAR + 1/4 * |SRTT - R'|
    //        <- RTTVAR + 1/4 (|SRTT - R'| - RTTVAR)
    int32_t mean_dev_update;
    if (rtt_error < 0) {
        // Linux uses a variation of the formula where the estimate is not
        // allowed to decrease too quickly in the case of decreasing RTTs:
        // RTTVAR <- (1 - alpha * beta) * RTTVAR + alpha * beta * |SRTT - R'|
        mean_dev_update = -rtt_error;  // is now abs()
        mean_dev_update -= (mean_dev_x4_ >> 2);
        if (mean_dev_update > 0) {
            mean_dev_update >>= 3;
        }
    } else {
        mean_dev_update = rtt_error - (mean_dev_x4_ >> 2);
    }
    mean_dev_x4_ += mean_dev_update;

    // If the mean deviation increased beyond the minimum RTO or the previous
    // maximum of the mean deviation we update immediately
    if (mean_dev_x4_ > max_mean_dev_x4_) {
        max_mean_dev_x4_ = mean_dev_x4_;
        if (mean_dev_x4_ > rtt_var_x4_) {
            rtt_var_x4_ = mean_dev_x4_;
        }
    }

    // Otherwise we update once per RTT and ensure that 
    if (tcp_util::After(sample.seq_acked_, next_seq_)) {
        if (max_mean_dev_x4_ < rtt_var_x4_) {
            rtt_var_x4_ -= (rtt_var_x4_ - max_mean_dev_x4_) >> 2;
        }
        next_seq_ = sample.seq_next_;
        max_mean_dev_x4_ = kMinRTOUs;
    }
}

uint32_t TcpTimer::GetRTO(uint8_t num_rtos) const {
    uint32_t rto;
    if (!smoothed_rtt_x8_) {
        // We don't have a sample yet, therefore return the default
        // conservative RTO
        rto = kMinRTOUs;
    } else if (kClockGranularityUs > rtt_var_x4_) {
        rto = (smoothed_rtt_x8_ >> 3) + kClockGranularityUs;
    } else {
        rto = (smoothed_rtt_x8_ >> 3) + rtt_var_x4_;
    }

    return AdjustRTOForBackoff(rto, num_rtos);
}

uint32_t TcpTimer::GetTLP(bool delayed_ack) const {
    uint32_t rtt = smoothed_rtt_x8_ >> 3;
    uint32_t tlp = rtt << 1;
    if (delayed_ack) {
        tlp = std::max(tlp, rtt + (rtt >> 1) + kMaxDelayedAckUs);
    }

    // TLP is scheduled instead of an RTO if the RTO would happen earlier
    return std::min(tlp, GetRTO());
}

uint32_t TcpTimer::AdjustRTOForBackoff(uint32_t rto, uint8_t num_rtos) {
    while (num_rtos) {
        rto <<= 1;
        if (rto > kMaxRTOUs) {
            return kMaxRTOUs;
        }
        num_rtos--;
    }

    return rto;
}
