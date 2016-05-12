#include "delay_analysis.h"

#include <glog/logging.h>
#include <utility>
#include <vector>

#include "tcp_packet.h"

// Configuration parameters (see delay_analysis.h for a detailed description of
// each parameter)
typedef DelayAnalysis DA;

constexpr float DA::kMinUnackedBytesRttCorrelation = 0.5;
constexpr uint32_t kBytesPerMicrosecondInBitsPerSecond = 8E6;

DelayAnalysis::DelayAnalysis(const TcpEndpoint& endpoint)
        : endpoint_(endpoint) {
    Clear();
}

void DelayAnalysis::Clear() {
    tail_latency_ = {0};
    first_packet_ = nullptr;
    worst_packet_ = nullptr;
    correlation_ = -1;
}


std::vector<TimerEstimates> DelayAnalysis::GetTimerEstimates(
        std::vector<uint32_t> relative_seqs) {
    std::vector<TimerEstimates> estimate_list;

    if (relative_seqs.empty() || worst_packet_ == nullptr) {
        return estimate_list;
    }
    if (correlation_ == -1 &&
            (!CalculateRttLinearFit(*worst_packet_) ||
             correlation_ < kMinUnackedBytesRttCorrelation)) {
        return estimate_list;
    }

    // Compute queue-free timers along the way
    TcpTimer queue_free_timer;
    auto rtt_samples = endpoint_.timer().samples();
    if (rtt_samples.empty()) {
        return estimate_list;
    }
    auto rtt_sample_iter = rtt_samples.begin();
    auto current_ack_index =
        rtt_sample_iter->packet_->tcp()->ack_packet()->index();

    // This assumes that the list of relative sequence numbers is already
    // ordered, and that a lower number is always found in the list of packets
    // earlier than a higher number.
    uint8_t index = 0;
    for (const Packet* packet : endpoint_.packets()) {
        const auto* tcp = packet->tcp();
        
        // Update timer with all RTT samples that came in before this packet
        while (rtt_sample_iter != rtt_samples.end() &&
                current_ack_index < packet->index()) {
            auto rtt_sample = *rtt_sample_iter;
            const int32_t queuing_delay_us =
                GetQueueingDelay(*(rtt_sample.packet_));
            if (rtt_sample.rtt_us_ > queuing_delay_us) {
                rtt_sample.rtt_us_ -= queuing_delay_us;
                queue_free_timer.AddSample(rtt_sample);
            }
            ++rtt_sample_iter;
            if (rtt_sample_iter != rtt_samples.end()) {
                current_ack_index =
                    rtt_sample_iter->packet_->tcp()->ack_packet()->index();
            }
        }

        if (!tcp_util::Before(tcp->relative_seq(), relative_seqs[index])) {
            TimerEstimates estimate;
            estimate.seq_ = relative_seqs[index];
            estimate.rto_us_ = tcp->rto_estimate_us();
            estimate.tlp_us_ = tcp->tlp_estimate_us();
            estimate.tlp_delayed_ack_us_ = tcp->tlp_delayed_ack_estimate_us();
            estimate.queue_free_rto_us_ = queue_free_timer.GetRTO();
            estimate.queue_free_tlp_us_ = queue_free_timer.GetTLP(false);
            estimate.queue_free_tlp_delayed_ack_us_ =
                queue_free_timer.GetTLP(true);
            estimate_list.push_back(estimate);
            if (++index >= relative_seqs.size()) {
                break;
            }
        }
    }

    // Push empty entries for sequence numbers not touched yet
    while (index < relative_seqs.size()) {
        TimerEstimates estimate = {0};
        estimate_list.push_back(estimate);
        index++;
    }

    return estimate_list;
}

Delays DelayAnalysis::AnalyzeTailLatency() {
    return AnalyzeTailLatency(0);
}

Delays DelayAnalysis::AnalyzeTailLatency(uint32_t max_relative_seq) {
    const std::vector<Packet*> packets = endpoint_.packets();

    Clear();
    if (packets.empty()) {
        VLOG(1) << "Endpoint has no packets";
        return tail_latency_;
    }

    // Find the packet with the worst ACK delay (latency)
    for (const Packet* packet : packets) {
        const auto* tcp = packet->tcp();
        if (max_relative_seq &&
                tcp_util::After(tcp->relative_seq(), max_relative_seq)) {
            break;
        }
        if (!tcp->data_len()) {
            continue;
        }
        if (!first_packet_) {
            first_packet_ = packet;
        }
        if (!worst_packet_ ||
                tcp->ack_delay_us() > worst_packet_->tcp()->ack_delay_us()) {
            worst_packet_ = packet;
        }
    }
    if (!worst_packet_) {
        VLOG(1) << "Endpoint has no packets with relative seq below "
                << max_relative_seq;
        return tail_latency_;
    }
    VLOG(3) << "Worst packet - Seq: " << worst_packet_->tcp()->seq();
    tail_latency_.bytes_unacked_ = worst_packet_->tcp()->unacked_bytes();

    ComputeGoodputMetrics();

    tail_latency_.overall_us_ = worst_packet_->tcp()->ack_delay_us();
    VLOG(1) << "Worst, overall (ms): " << tail_latency_.overall_us_ / 1000;
    if (!tail_latency_.overall_us_) {
        return tail_latency_;
    }
    tail_latency_.propagation_us_ = endpoint_.min_rtt_us();
    VLOG(1) << "Worst, propagation (ms): " << tail_latency_.propagation_us_ / 1000;

    // Tie the delay to a retransmission delay if the packet is lost
    if (worst_packet_->IsLost()) {
        tail_latency_.loss_us_ = worst_packet_->tcp()->final_rtx_delay_us();
        tail_latency_.time_to_first_rtx_us_ =
            worst_packet_->rtx()->timestamp_us() - worst_packet_->timestamp_us();
    }
    VLOG(1) << "Worst, overall loss (ms): " << tail_latency_.loss_us_ / 1000;

    // Compute the fraction of the delay that can be attributed to queueing
    // (either directly or related to the delay of the trigger packet)
    if (CalculateRttLinearFit(*worst_packet_) &&
            correlation_ > kMinUnackedBytesRttCorrelation) {
        // Queueing delay is determined by the packet that reached the receiver
        VLOG(2) << "Connection has high rtt/flight correlation";
        const Packet* last_tx = worst_packet_;
        while (last_tx->IsLost()) {
            last_tx = last_tx->rtx();
        }
        tail_latency_.queueing_us_ = GetQueueingDelay(*last_tx);
        tail_latency_.loss_trigger_breakdown_ = GetTriggerDelay(*worst_packet_);
        tail_latency_.loss_trigger_us_ = tail_latency_.loss_trigger_breakdown_.total();
    }
    VLOG(1) << "Worst, trigger (ms): " << tail_latency_.loss_trigger_us_ / 1000;
    VLOG(1) << "Worst, queueing (ms): " << tail_latency_.queueing_us_ / 1000;

    // TODO check other causes

    // Enforce constraints (delay combinations cannot exceed overall delay)
    const uint32_t non_prop_loss_us =
        tail_latency_.overall_us_ - tail_latency_.loss_us_ -
        tail_latency_.propagation_us_;
    if (tail_latency_.queueing_us_ > non_prop_loss_us) {
        tail_latency_.queueing_us_ = non_prop_loss_us;
    }

    // Assign only non-trigger delay to the pure loss counter
    uint32_t base_loss_us =
        tail_latency_.loss_trigger_breakdown_.no_queue_timeout_us_;
    if (tail_latency_.loss_trigger_breakdown_.late_ack_arms_us_ ||
        tail_latency_.loss_trigger_breakdown_.late_ack_triggers_us_) {
        base_loss_us += tail_latency_.propagation_us_;
    }

    if (tail_latency_.loss_us_ < tail_latency_.loss_trigger_us_) {
        tail_latency_.loss_us_ = 0;
    } else {
        tail_latency_.loss_us_ -= tail_latency_.loss_trigger_us_;
    }

    if (tail_latency_.loss_us_ < base_loss_us) {
        uint32_t diff = base_loss_us - tail_latency_.loss_us_;
        tail_latency_.loss_us_ += diff;
        tail_latency_.loss_trigger_us_ -= diff;
    }

    VLOG(1) << "Worst, remaining loss (ms): " << tail_latency_.loss_us_ / 1000;

    // Store delay that has not been accounted for
    tail_latency_.SetOtherDelay();
    VLOG(1) << "Worst, other (ms): " << tail_latency_.other_us_ / 1000;

    return tail_latency_;
}

void DelayAnalysis::ComputeGoodputMetrics() {
    auto acked_bytes = worst_packet_->tcp()->acked_bytes();
    auto elapsed_time_us =
        worst_packet_->timestamp_us() - first_packet_->timestamp_us();
    if (!elapsed_time_us) {
        VLOG(3) << "No time elapsed up to worst packet";
        return;
    }
    tail_latency_.bytes_acked_before_worst_packet_ = acked_bytes;
    tail_latency_.goodput_before_worst_packet_bps_ =
        acked_bytes * kBytesPerMicrosecondInBitsPerSecond / elapsed_time_us;
    VLOG(3) << "Bytes acked before worst packet: "
            << tail_latency_.bytes_acked_before_worst_packet_
            << ", elapsed (us): "
            << elapsed_time_us;

    // To continue consuming the achieved goodput rate once we reach the worst
    // packet, we need to have data buffered, but can consume additional
    // in-order acked data as well. To get the amount of data needed buffered we
    // start with a zero balance and keep the track of the largest balance seen
    // along the way.
    const auto* start_ack = worst_packet_->tcp()->last_ack();
    const auto* end_ack = worst_packet_->tcp()->ack_packet();
    if (start_ack == nullptr || end_ack == nullptr) {
        VLOG(3) << "Cannot compute buffer size due to missing ACKs";
        return;
    }

    int32_t buffer_needed = 0;
    int32_t max_buffer_needed = 0;
    auto current_ack_no = start_ack->tcp()->ack();
    auto current_timestamp = worst_packet_->timestamp_us();
    const auto* current_ack = start_ack->next_packet();
    while (current_ack != end_ack && current_ack != nullptr) {
        auto elapsed_time_us = current_ack->timestamp_us() - current_timestamp;
        current_timestamp = current_ack->timestamp_us();

        buffer_needed += elapsed_time_us * 
            tail_latency_.goodput_before_worst_packet_bps_ /
            kBytesPerMicrosecondInBitsPerSecond;
        if (buffer_needed > max_buffer_needed) {
            max_buffer_needed = buffer_needed;
        }
        if (tcp_util::After(current_ack->tcp()->ack(), current_ack_no)) {
            buffer_needed -= current_ack->tcp()->ack() - current_ack_no;
            current_ack_no = current_ack->tcp()->ack();
        }
        current_ack = current_ack->next_packet();
    }

    tail_latency_.bytes_needed_buffered_ = max_buffer_needed;
}

bool DelayAnalysis::CalculateRttLinearFit(const Packet& packet) {
    bool found_fit = false;
    double current_correlation;
    stats_util::LinearFitParameters current_fit;

    // Try fitting based on multiple sample sets and use the one with the
    // highest correlation factor
    std::vector<std::pair<bool, bool>> option_combos =
            { {false, false}, {true, false}, {true, true} };
    for (auto option_combo : option_combos) {
        VLOG(2) << "Trying to find linear fit...";
        if (GetRttLinearFit(
                    &current_fit, &current_correlation, packet,
                    option_combo.first, option_combo.second) &&
                current_correlation > correlation_) {
            fit_ = current_fit;
            correlation_ = current_correlation;
            found_fit = true;
            VLOG(2) << "Current correlation: " << correlation_;
        }
    }

    return found_fit;
}

bool DelayAnalysis::GetRttLinearFit(
        stats_util::LinearFitParameters* fit,
        double* correlation,
        const Packet& packet,
        bool use_packets_around_only,
        bool use_older_packets_only) {
    auto bytes_rtt_pairs =
        use_packets_around_only ?
            endpoint_.GetUnackedBytesRttPairsAroundPacket(
                    packet, 60, use_older_packets_only) :
            endpoint_.GetUnackedBytesRttPairs();
    if (bytes_rtt_pairs.empty()) {
        VLOG(2) << "No byte/RTT pairs for fitting";
        return false;
    }
    
    std::vector<double> rtts, unacked_bytes;
    vector_util::SplitPairs(bytes_rtt_pairs, &unacked_bytes, &rtts);

    // Check if the correlation between the number of unacked bytes and the RTT
    // is high enough (i.e. pending byte counts impact RTTs)
    *correlation = stats_util::PearsonCorrelation(unacked_bytes, rtts);

    // Use regression to find the best linear fit with a constant term
    *fit = stats_util::LinearFit(unacked_bytes, rtts);
   
    // The linear fit is only useful if it has a positive slope (i.e. with
    // growing number of unacked bytes the RTT grows too)
    return (fit->c_1 > 0);
}

uint32_t DelayAnalysis::GetQueueingDelay(const Packet& packet) const {
    // Compute the estimated RTT based on the given number of unacked bytes
    // when the given packet was transmitted
    if (packet.tcp()->unacked_bytes() <= packet.tcp()->data_len()) {
        return 0;
    }
    const auto bytes_before_tx =
        packet.tcp()->unacked_bytes() - packet.tcp()->data_len();
    const double estimated_rtt_us =
        stats_util::LinearFitValueForX(fit_, bytes_before_tx);
    
    // Get the fraction of the RTT attributed to queueing.
    // We subtract the maximum chosen from the estimated y-intersect
    // (estimated RTT when no data is unacked) and the minimum RTT observed
    const double prop_delay_us = tail_latency_.propagation_us_;
    const double min_delay_us = std::max(fit_.c_0, prop_delay_us);

    if (min_delay_us < estimated_rtt_us) {
        return estimated_rtt_us - min_delay_us;
    } else {
        return 0;
    }
}

TriggerDelays DelayAnalysis::GetTriggerDelay(const Packet& packet) {
    TriggerDelays delays = {0};
    // Get the transmission that reached the receiver and then work backwards
    // towards the first transmission (via trigger packets or RTOs)
    if (!packet.IsLost()) {
        return delays;
    }
    const Packet* last_tx = packet.rtx();
    while (last_tx->IsLost()) {
        last_tx = last_tx->rtx();
    }

    // Recompute the timeouts for this connection assuming no queueing delays
    // and determine the delays from inflated RTO and TLP timers
    if (no_queue_timeouts_.empty()) {
        ComputeQueueFreeTimeouts();
    }
    const Packet* current_tx = last_tx;
    while (current_tx->previous_tx() != nullptr) {
        // If the packet has a trigger packet associated with it, the trigger delay
        // equals the queueing delay of the trigger packet (e.g. for fast
        // retransmissions) and the trigger delay of the trigger packet (e.g.
        // possible for multiple rounds of slow-start retransmissions)
        if (current_tx->trigger_packet() != nullptr) {
            delays.late_ack_triggers_us_ =
                GetQueueingDelay(*(current_tx->trigger_packet()));
            VLOG(2) << "Queueing delay of trigger packet (ms): "
                    << delays.late_ack_triggers_us_ / 1000;

            if (current_tx->tcp()->is_slow_start_rtx()) {
                delays.late_trigger_for_trigger_us_ =
                    GetTriggerDelay(*(current_tx->trigger_packet()->first_tx())).total();
                VLOG(2) << "Trigger delay (slow start, ms):"
                        << delays.late_trigger_for_trigger_us_ / 1000;
            }
            return delays;
        }

        uint32_t no_queue_timeout = 0;
        uint32_t actual_timeout = 0;
        
        if (current_tx->tcp()->is_rto_rtx()) {
            // Current transmission was triggered by an RTO. Figure out how much
            // the RTO was bloated up by queueing delay.
            no_queue_timeout = GetQueueFreeRTO(*current_tx);
            actual_timeout = current_tx->tcp()->rto_info().delay_us_;
            VLOG(3) << "Found RTO timeout";
        } else if (current_tx->tcp()->is_tlp()) {
            // Current transmission is a TLP. Figure out how much the TLP was
            // delayed by earlier queueing delay
            if (current_tx->tcp()->tlp_info().delayed_ack_) {
                no_queue_timeout = GetQueueFreeDelayedTLP(*current_tx);
            } else {
                no_queue_timeout = GetQueueFreeTLP(*current_tx);
            }
            actual_timeout = current_tx->tcp()->tlp_info().delay_us_;
            VLOG(3) << "Found TLP timeout";
        } else {
            VLOG(3) << "Found neither RTO nor TLP timeout indication";
        }
        VLOG(2) << "NQ timeout: " << no_queue_timeout / 1000
                 << ", actual: " << actual_timeout / 1000;
        if (no_queue_timeout) {
            delays.no_queue_timeout_us_ += no_queue_timeout;
            if (actual_timeout > no_queue_timeout) {
                delays.timeout_us_ += actual_timeout - no_queue_timeout;
                VLOG(2) << "Timeout difference (ms): "
                        << (actual_timeout - no_queue_timeout) / 1000;
            }
        }

        // If the RTO or TLP timer was rearmed due to an incoming ACK the loss
        // trigger is further delayed by the queueing delay of the packet
        // triggering that ACK
        delays.late_ack_arms_us_ += GetArmingTimerDelay(*current_tx);
        VLOG(2) << "Arming timer delay (ms): "
                << delays.late_ack_arms_us_ / 1000;

        current_tx = current_tx->previous_tx();
    }

    return delays;
}

uint32_t DelayAnalysis::GetArmingTimerDelay(const Packet& packet) const {
    // Arming the timer is only delayed if it was caused by an ACK
    const Packet* armer;
    if (packet.tcp()->is_rto_rtx()) {
        armer = packet.tcp()->rto_info().armed_by_;
    } else if (packet.tcp()->is_tlp()) {
        armer = packet.tcp()->tlp_info().armed_by_;
    } else {
        return 0;
    }

    if (armer == nullptr) {
        return 0;
    }
    if (armer->IsFromSameEndpoint(packet)) {
        return 0;
    }
    if (armer->trigger_packet() == nullptr) {
        return 0;
    }

    // Get the packet that triggered the ACK and return its queueing delay
    return GetQueueingDelay(*(armer->trigger_packet()));
}

void DelayAnalysis::ComputeQueueFreeTimeouts() {
    TcpTimer timer;
    std::vector<RttSample> rtt_samples = endpoint_.timer().samples();

    // Add every RTT sample to a new timer with the queueing
    // delay subtracted
    no_queue_timeouts_.clear();
    for (RttSample rtt_sample : rtt_samples) {
        const int32_t queueing_delay_us = GetQueueingDelay(*(rtt_sample.packet_));
        if (rtt_sample.rtt_us_ > queueing_delay_us) {
            rtt_sample.rtt_us_ -= queueing_delay_us;
            timer.AddSample(rtt_sample);

            // The new timeout values are used for any packets sent out
            // after the ACK for the given packet. We don't know the number of
            // unacked packets here, so we compute the TLP assuming there are
            // more than one outstanding packet and fix that later if necessary
            uint32_t ack_index =
                rtt_sample.packet_->tcp()->ack_packet()->index();
            auto current_timeouts =
                std::make_tuple(
                        ack_index,
                        timer.GetRTO(),
                        timer.GetTLP(false),
                        timer.GetTLP(true));
            no_queue_timeouts_.push_back(current_timeouts);
        }
    }
}

void DelayAnalysis::GetQueueFreeTimeouts(const Packet& packet,
        uint32_t* rto, uint32_t* tlp, uint32_t* delayed_tlp) {
    // Get the packet which armed the timer
    const Packet* armer;
    if (packet.tcp()->is_tlp()) {
        armer = packet.tcp()->tlp_info().armed_by_;
    } else if (packet.tcp()->is_rto_rtx()) {
        armer = packet.tcp()->rto_info().armed_by_;
    } else {
        return;
    }
    if (armer == nullptr) {
        return;
    }

    // Find the matching timeout estimate in the list of recomputed
    // (queue-free) timeouts, i.e. the entry with the highest index
    // below the index of the armer packet
    for (auto it = no_queue_timeouts_.rbegin();
            it != no_queue_timeouts_.rend(); ++it) {
       IndexTimeouts timeouts = *it;
       if (std::get<0>(timeouts) < armer->index()) {
           // Found the matching timeout estimates
           *rto = std::get<1>(timeouts);
           *tlp = std::get<2>(timeouts);
           *delayed_tlp = std::get<3>(timeouts);
           return;
       }
    }
}

uint32_t DelayAnalysis::GetQueueFreeRTO(const Packet& packet) {
    uint32_t rto = 0;
    uint32_t unused;
    const uint8_t num_rtos = packet.tcp()->rto_info().backoffs_;
    GetQueueFreeTimeouts(packet, &rto, &unused, &unused);
    return TcpTimer::AdjustRTOForBackoff(rto, num_rtos);
}

uint32_t DelayAnalysis::GetQueueFreeTLP(const Packet& packet) {
    uint32_t tlp = 0;
    uint32_t unused;
    GetQueueFreeTimeouts(packet, &unused, &tlp, &unused);
    return tlp;
}

uint32_t DelayAnalysis::GetQueueFreeDelayedTLP(const Packet& packet) {
    uint32_t tlp = 0;
    uint32_t unused;
    GetQueueFreeTimeouts(packet, &unused, &unused, &tlp);
    return tlp;
}
