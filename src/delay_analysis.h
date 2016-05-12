#ifndef DELAY_ANALYSIS_H_
#define DELAY_ANALYSIS_H_

#include <tuple>

#include "tcp_endpoint.h"
#include "util.h"

typedef std::tuple<uint32_t, uint32_t, uint32_t, uint32_t> IndexTimeouts;

typedef struct {
    uint32_t seq_;

    // Timer estimates based on observed RTT samples
    // 1. Classic retransmission timeout (RTO)
    // 2. Tail loss probe (TLP) when packets_out > 1
    // 3. Tail loss probe (TLP) when packets_out == 1 (adjusts for the delayed
    // ACK timer)
    uint32_t rto_us_;
    uint32_t tlp_us_;
    uint32_t tlp_delayed_ack_us_;

    // Timer estimated based on observed and adjusted RTT samples (subtracted
    // estimated queuing delay for each delayed packet)
    uint32_t queue_free_rto_us_;
    uint32_t queue_free_tlp_us_;
    uint32_t queue_free_tlp_delayed_ack_us_;
} TimerEstimates;

typedef struct {
    // Base timeout value derived from recomputing it assuming that no RTT
    // samples suffered from queuing delays
    uint32_t no_queue_timeout_us_;

    // Delay due to an inflated timeout value
    uint32_t timeout_us_;

    // Delay due to a late incoming ACK that armed the timer (i.e.
    // retransmission is ultimately caused by the timeout)
    uint32_t late_ack_arms_us_;

    // Delay due to a late incoming ACK that triggered the retransmission
    uint32_t late_ack_triggers_us_;

    // Delay of triggering the trigger packet itself (e.g. accumulation of
    // delays when multiple rounds of slow-start retransmissions happen)
    uint32_t late_trigger_for_trigger_us_;

    uint32_t total() {
        return timeout_us_ + late_ack_arms_us_ + late_ack_triggers_us_ +
            late_trigger_for_trigger_us_;
    }
} TriggerDelays;

typedef struct {
    // Overall delay (elapsed time between first transmission
    // and ACK (can include retransmissions)
    uint32_t overall_us_;

    // Delay attributed to propagation based on the minimum RTT observed in the
    // connection
    uint32_t propagation_us_;

    // Delay caused by a packet loss that cannot be attributed to a different
    // issue (e.g. queueing delay for a different packet that served as the
    // retransmission trigger)
    uint32_t loss_us_;

    // Delay between the original transmission and the first retransmission. If
    // there are no retransmissions, this value is zero (does NOT subtract
    // queuing delays tied to triggers)
    uint32_t time_to_first_rtx_us_;

    // Delay caused by "trigger packets" being delayed (i.e. the packet loss was
    // not detected earlier due to packets triggering the recovery being
    // delayed)
    uint32_t loss_trigger_us_;
    TriggerDelays loss_trigger_breakdown_;

    // Queueing delay (i.e. estimated amount of time that the packet spent in a
    // queue)
    uint32_t queueing_us_;

    // Remaining delay that is not accounted for by any of the above categories
    uint32_t other_us_;

    uint64_t goodput_before_worst_packet_bps_;
    uint64_t bytes_acked_before_worst_packet_;
    uint64_t bytes_needed_buffered_;

    uint32_t bytes_unacked_;

    void SetOtherDelay() {
        const uint32_t all_delays_us =
            propagation_us_ + loss_us_ + loss_trigger_us_ + queueing_us_;
        if (all_delays_us > overall_us_) {
            other_us_ = 0;
        } else {
            other_us_ = overall_us_ - all_delays_us;
        }
    }
} Delays;

class DelayAnalysis {
    public:
        // Minimum correlation coefficient required to attribute some delay to
        // queueing (actual delay attributed depends on the linear fit)
        static const float kMinUnackedBytesRttCorrelation;

        DelayAnalysis(const TcpEndpoint& endpoint);

        // Returns a list of timer estimates (RTOs, TLPs, including/excluding
        // queuing delays). Given a SORTED list of relative sequence numbers,
        // for each number the first packet with a number equal/larger will be
        // selected and its corresponding timer values returned
        std::vector<TimerEstimates> GetTimerEstimates(
                std::vector<uint32_t> relative_seqs);

        Delays AnalyzeTailLatency();

        Delays AnalyzeTailLatency(uint32_t max_relative_seq);

        inline stats_util::LinearFitParameters fit() const {
            return fit_;
        }
        inline double correlation() const {
            return correlation_;
        }

    private:
        void Clear();

        void ComputeGoodputMetrics();

        // Generates a linear fit based on the <# unacked bytes, RTT> samples in
        // this connection. Returns TRUE, if a linear fit was generated and
        // stored in the output argument
        bool CalculateRttLinearFit(const Packet& packet);

        bool GetRttLinearFit(
                stats_util::LinearFitParameters* fit,
                double* correlation,
                const Packet& packet,
                bool use_packets_around_only,
                bool use_older_packets_only);

        // Extrapolates the queueing delay likely observed by the given packet
        // using the given linear fit for # unacked bytes vs. RTT
        uint32_t GetQueueingDelay(const Packet& packet) const;

        // Extrapolates the trigger delays which equal the impact of queueing of
        // earlier packets on the transmission time of the current packet (e.g.
        // for a single fast retransmission the trigger delay equals the queueing
        // delay of the data packet that caused a SACK which then triggered the
        // fast retransmission.
        TriggerDelays GetTriggerDelay(const Packet& packet);

        // If the given packet is an RTO retransmission or a TLP, this computes
        // the impact of queueing delay on arming the timer (arming can be
        // delayed if it is caused by an ACK with a corresponding trigger
        // packet)
        uint32_t GetArmingTimerDelay(const Packet& packet) const;

        // Recomputes the RTO and TLP timeouts that the connection would have
        // observed in the absence of queueing delay.
        // Returns tuples of <index, RTO estimate, TLP timeout estimate> where
        // the index is the index of the first packet after which these
        // timeouts would be used
        void ComputeQueueFreeTimeouts();

        // Returns the estimated RTO and TLP timeouts ignoring any queueing delays  
        void GetQueueFreeTimeouts(const Packet& packet, uint32_t* rto, uint32_t* tlp,
                uint32_t* delayed_tlp);
        
        uint32_t GetQueueFreeRTO(const Packet& packet);
        uint32_t GetQueueFreeTLP(const Packet& packet);
        uint32_t GetQueueFreeDelayedTLP(const Packet& packet);
        
        const TcpEndpoint& endpoint_;
        const Packet* first_packet_;
        const Packet* worst_packet_;

        Delays tail_latency_;

        stats_util::LinearFitParameters fit_;
        double correlation_;

        // Stores tuples of <index, RTO estimate, TLP timeout estimate> where
        // the index is the index of the first packet after which these
        // timeouts would be used
        std::vector<IndexTimeouts> no_queue_timeouts_;
};

#endif  /* DELAY_ANALYSIS_H_ */
