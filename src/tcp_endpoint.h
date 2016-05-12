#ifndef TCP_ENDPOINT_H_
#define TCP_ENDPOINT_H_

#include <queue>
#include <utility>
#include <vector>

#include "packet.h"
#include "tcp_sacks.h"
#include "tcp_timer.h"

class TcpEndpoint {
    public:
        // Maximum delay between a packet A and B, s.t. packet B is still
        // considered to be trigger by the reception of packet A
        static const uint64_t kMaxTriggerPacketDelayUs;

        explicit TcpEndpoint(Packet* packet);

        inline uint32_t addr() const {
            return addr_;
        }
        inline uint16_t port() const {
            return port_;
        }
        inline std::vector<Packet*> packets() const {
            return packets_;
        }
        inline TcpTimer timer() const {
            return timer_;
        }
        inline uint32_t min_rtt_us() const {
            return min_rtt_us_;
        }
        inline bool is_bogus() const {
            return is_bogus_;
        }
        uint32_t GetUnackedBytes() const {
            if (sacks_.num_bytes() > seq_next_ - seq_acked_) {
                return 0;
            } else {
                return seq_next_ - seq_acked_ - sacks_.num_bytes();
            }
        }

        // Initialize state relying on sequence numbers (once negotiated).
        // Relative sequence and ACK numbers start at 1.
        void SetInitialSequenceNumbers();

        // Adds a new packet that was transmitted by this endpoint and updates
        // the internal state if process_packet is to TRUE (includes tracking
        // unacked data, ACKs, etc.).
        // Returns the inferred list of on-the-wire-packets.
        std::vector<Packet*> AddPacket(Packet* packet, bool process_packet);

        // Derive the maximum segment size (MSS) for data from this endpoint by
        // either extracting the MSS option from this packet (if it is a SYN) or
        // by estimating the MSS assuming that the sender used a multiple of the
        // MSS for the length of this packet.
        void DeriveMSS();

        // Splits a possibly larger-than-MSS packet into wire-sized packets (if
        // the MSS is known)
        std::vector<Packet*> SplitIntoWirePackets();

        // Process the ACK (e.g. mark unacked packets as acked) and possible
        // SACK and DSACK blocks
        void ProcessAck(Packet* packet);

        // Go through the list of unacked packets and remove the ones that are
        // now acked
        void AckPackets();

        // SACKs with ranges below the current ACK number are DSACKs indicating
        // that earlier retransmissions were spurious. We add a tag to the
        // falsely retransmitted packets
        void DSackPackets();

        // Find the most recently packet carrying the given sequence range that
        // was marked as retransmitted, and add the retransmission tag
        void HandleSpuriousRtx(uint32_t seq_start, uint32_t seq_end);

        // Returns the number of packets that were marked as lost
        uint32_t GetNumLosses() const;

        // Returns the number of packets carrying data/payload
        uint32_t GetNumDataPackets() const;

        // Returns the number of packets with SACK blocks
        uint32_t GetNumSackPackets() const;

        // Returns the number of retransmissions for which we did not find a
        // trigger
        uint32_t GetNumMissingTriggerPackets() const;

        // Returns the measured RTTs based on the ACK delays for packets received
        // in-order and that were not retransmitted
        std::vector<uint32_t> GetRttsUs() const;

        // Returns the measured ACK delays for data packets (preliminary ACKing
        // by SACK blocks is not considered)
        std::vector<uint32_t> GetAckDelaysUs() const;

        // Returns the unacked byte counts observed for every data packet
        // transmitted
        std::vector<uint32_t> GetUnackedByteCounts() const;

        // Computes the number of bytes already received successfully by the
        // other endpoint (for each packet transmitted by this endpoint). This
        // includes packets that are potentially still in flight while
        // transmitting the current packet
        void SetPassedBytesForPackets();

        // Computes the goodput for this endpoint (all outgoing data) as the
        // amount of data acked divided by the time elapsed between the first
        // data packet and the last ACK for any of the data (excludes dupacks).
        // If cut_off_at_loss is TRUE, goodput is only computed for data packets
        // up to the first loss
        uint64_t GetGoodputBps(bool cut_off_at_loss) const;
        
        // Collects the number of unacked bytes for each RTT sample (includes
        // only data from packets that were transmitted in-order and were not
        // lost)
        std::vector<std::pair<double, double>>
            GetUnackedBytesRttPairs() const;
        
        std::vector<std::pair<double, double>>
            GetUnackedBytesRttPairsAroundPacket(
                    const Packet& target_packet,
                    uint8_t num_samples,
                    bool use_older_packets_only) const;

        // Helper methods to calculate the number of packets where the given
        // function returns a non-zero value (e.g. data length, has SACKs, ...)
        template<typename ReturnType>
        using PacketFunction = ReturnType(Packet::*)() const;

        template<typename ReturnType>
        using TcpPacketFunction = ReturnType(TcpPacket::*)() const;

        template<typename ReturnType>
        uint32_t GetCountForNonZeroFunctionValue(
                PacketFunction<ReturnType> func) const;
        
        template<typename ReturnType>
        uint32_t GetCountForNonZeroFunctionValue(
                TcpPacketFunction<ReturnType> func) const;

        // Helper method to collect the values returned by a particular function
        // for all packets
        // Example: CollectFieldValues(&TcpPacket::ack_delays_us)
        template<typename ReturnType>
        std::vector<ReturnType> CollectFunctionValues(
                TcpPacketFunction<ReturnType> func) const;

    private:
        // Recompute the TLP and RTO timeouts and set the given packet as the
        // trigger
        void ArmTimers(const Packet* packet);

        // Treat the given packet as a retransmission and annotate (includes
        // linking to the previous transmission, trigger packet, etc.)
        void ProcessRtx();

        // Check if the retransmission is caused by an RTO (based on the
        // estimated time when the RTO fires/fired), and if so update the timer,
        // including backoff;
        // Returns TRUE, if the current packet is triggered by an RTO
        bool CheckForRTO();


        // Check if the current packet is a tail loss probe (TLP)
        // Returns TRUE, if so
        bool CheckForTLP();

        // Determine if the retransmission has a trigger packet (i.e. the
        // reception of packet, like a dupack, triggered this)
        // For this we heuristically compare the delay between the last ACK
        // and this packet and assume that there is a causal relationship if the
        // delay is negligible
        // Returns TRUE, if a trigger was found
        bool FindRtxTrigger();

        // Process a packet that is acked by the given ACK packet. Does NOT arm
        // timers!
        void HandleAckedPacket(Packet* packet, Packet* ack);

        // We determined that his packet was sacked, now we find the earliest
        // SACK lookalike that was (probably) responsible for this.
        // Returns TRUE, if we matched a SACK lookalike and sacked the packet
        bool TiePacketToSackLookalike(Packet* packet);

        // If the current packet is a retransmission all older non-retransmitted
        // packet should have been retransmitted before. The ones that weren't
        // were likely sacked by lookalikes (i.e. SACK blocks were truncated
        // from ACKs)
        void CheckForSacksFromLookalikes();

        // For each data packet since the given packet add 'offset' to its
        // unacked_bytes count
        void AdjustUnackedBytesCountsAfter(const Packet& packet, int32_t offset);

        // Look for the most recent packet that carried (at least) the same
        // starting sequence number and mark this packet as its retransmission
        // Returns TRUE, if a previous transmission was found
        bool LinkToPreviousTx();

        // Mark packets between the given packet and its original transmission as
        // out-of-order
        void MarkPacketsOutOfOrder();

        const uint32_t addr_;
        const uint16_t port_;

        // (Estimated) maximum segment size allowed for this endpoint to
        // transmit
        uint32_t mss_ = 0;

        // All packets transmitted by this endpoint
        std::vector<Packet*> packets_;

        // Packets that have not been acknowledged yet (SACKed packets are
        // treated as acknowledged even though SACK reneging can happen)
        std::vector<Packet*> unacked_packets_;

        // If header options are truncated we may end up with dupacks which have
        // their SACK blocks cut off. Here we store the lookalikes that haven't
        // been tied to likely sacked packets yet.
        std::queue<Packet*> unused_sack_lookalikes_;

        // Last ACK received from the other endpoint
        Packet* last_ack_ = nullptr;

        // Last ACK to which we could assign a trigger packet. This should be
        // true for most ACKs (excludes for example ACKs with SACKs
        // where we didn't capture the SACKs, or pure window updates)
        Packet* last_ack_with_trigger_ = nullptr;

        // Most recent packet transmitted by this endpoint
        Packet* current_packet_ = nullptr;

        // Sequence ranges that have been SACKed (but not yet ACKed). This
        // possibly combines SACK blocks seen in multiple ACKs
        TcpSacks sacks_;

        // Tracks RTT samples and computes the smoothed RTT required for timeout
        // computation
        TcpTimer timer_;

        // References for the currently active RTO and TLP timers
        TcpTimerInfo rto_ = {nullptr, 0, 0, false};
        TcpTimerInfo tlp_ = {nullptr, 0, 0, false};

        // Current number of consecutive RTOs (without any ACKs)
        uint8_t num_rtos_ = 0;

        // The seq_next_ seen when the RTO expired. We use this to decide when
        // the recovery period ends, and to recover the seq_next_ value in case
        // we wrongly inferred than an RTO happened (e.g. when discovering TLP
        // instead)
        uint32_t rto_high_seq_ = 0;

        // Number of data packets transmitted by this endpoint (i.e. packets
        // carrying a payload)
        uint32_t num_data_packets_ = 0;

        // Number of data packets that are in flight (transmitted but not yet
        // acked or marked as lost)
        uint32_t num_in_flight_ = 0;
        
        // Highest sequence acked
        uint32_t seq_acked_ = 0;

        // Next in-order sequence to transmit (does not include possible
        // retransmissions)
        uint32_t seq_next_ = 0;

        // Initial sequence number (i.e. negotiated during SYN exchange or the
        // first sequence number seen in case of partially captured connections)
        uint32_t seq_init_ = 0;

        // Currently cumulatively ACKed sequence
        uint32_t ack_ = 0;

        // Initial ACK number (see seq_init_ above)
        uint32_t ack_init_ = 0;

        // Number of sent bytes (in-order) acked. We don't simply use the
        // difference between initial and current sequence numbers to protect
        // against wraparound
        uint64_t acked_bytes_ = 0;

        // TRUE, if both, initial sequence number and ACK number, have been set
        bool seq_initialized_ = false;

        // Minimum RTT observed so far
        uint32_t min_rtt_us_ = 0;

        // Number of data packets that were flagged as retransmissions, but for
        // which we could not find an earlier transmission
        uint32_t unmatched_rtx_ = 0;

        // TRUE, if there is any indication that we have a corrupted capture
        // (e.g. when observing ACKs before their corresponding data packets)
        bool is_bogus_ = false;

        // TRUE, if tail loss probes (TLPs) are enabled. This is a kernel
        // setting and not negotiated in the handshake. Finding out if TLP is
        // enabled requires at least one retransmission where the RTO and TLP
        // timeout are not the same
        bool is_tlp_enabled_ = true;

        friend class TcpFlow;
};

// Include definitions for templated functions
#include "tcp_endpoint.tcc"

#endif  /* TCP_ENDPOINT_H_ */
