#ifndef TCP_PACKET_H_
#define TCP_PACKET_H_

#include <arpa/inet.h>
#include <memory>
#include <netinet/tcp.h>
#include <pcap.h>
#include <string>
#include <vector>

#include "tcp_sacks.h"
#include "tcp_timer_info.h"

// Define some extra header flags that are not part of netinet/tcp.h
#define TH_ECE  0x40
#define TH_CWR  0x80

class Packet;

class TcpPacket {
    public:
        TcpPacket(u_char* packet, const uint32_t len, const uint32_t caplen);

        inline const u_char* packet() const {
            return packet_;
        }
        inline const uint16_t src_port() const {
            return ntohs(header_->th_sport);
        }
        inline const uint16_t dst_port() const {
            return ntohs(header_->th_dport);
        }
        inline const uint32_t data_offset() const {
            return header_->th_off << 2;
        }
        inline const uint32_t data_len() const {
            return len_ - data_offset();
        }
        inline const uint32_t seq() const {
            return ntohl(header_->th_seq);
        }
        inline const uint32_t seq_end() const {
            return seq() + data_len();
        }
        inline const uint32_t ack() const {
            return ntohl(header_->th_ack);
        }
        inline const uint32_t relative_seq() const {
            return relative_seq_;
        }
        inline const uint32_t relative_ack() const {
            return relative_ack_;
        }
        inline const uint8_t flags() const {
            return header_->th_flags;
        }
        inline const Packet* ack_packet() const {
            return ack_packet_;
        }
        inline const Packet* last_ack() const {
            return last_ack_;
        }
        inline TcpSacks sacks() const {
            return sacks_;
        }
        inline bool has_sack() const {
            return sacks_.num_sacks() > 0;
        }
        inline uint32_t num_sacks() const {
            return sacks_.num_sacks();
        }
        inline uint16_t mss_opt_value() const {
            return mss_opt_value_;
        }
        inline bool timestamp_ok() const {
            return timestamp_ok_;
        }
        inline bool is_bogus() const {
            return is_bogus_;
        }
        inline bool is_dupack() const {
            return is_dupack_;
        }
        inline uint16_t unknown_option_size() const {
            return unknown_option_size_;
        }
        inline bool is_rtx() const {
            return is_rtx_;
        }
        inline bool is_spurious_rtx() const {
            return is_spurious_rtx_;
        }
        inline bool is_fast_rtx() const {
            return is_fast_rtx_;
        }
        inline bool is_rto_rtx() const {
            return is_rto_rtx_;
        }
        inline bool is_slow_start_rtx() const {
            return is_slow_start_rtx_;
        }
        inline bool is_tlp() const {
            return is_tlp_;
        }
        inline uint32_t ack_delay_us() const {
            return ack_delay_us_;
        }
        inline uint32_t rtx_delay_us() const {
            return rtx_delay_us_;
        }
        inline uint32_t final_rtx_delay_us() const {
            return final_rtx_delay_us_;
        }
        inline uint32_t unacked_bytes() const {
            return unacked_bytes_;
        }
        inline uint64_t acked_bytes() const {
            return acked_bytes_;
        }
        inline uint16_t num_rtx_attempts() const {
            return num_rtx_attempts_;
        }
        inline TcpTimerInfo rto_info() const {
            return rto_info_;
        }
        inline TcpTimerInfo tlp_info() const {
            return tlp_info_;
        }
        inline uint32_t rto_estimate_us() const {
            return rto_estimate_us_;
        }
        inline uint32_t tlp_estimate_us() const {
            return tlp_estimate_us_;
        }
        inline uint32_t tlp_delayed_ack_estimate_us() const {
            return tlp_delayed_ack_estimate_us_;
        }
        inline bool IsAck() const {
            return flags() & TH_ACK;
        }
        inline bool IsFin() const {
            return flags() & TH_FIN;
        }
        inline bool IsSyn() const {
            return flags() & TH_SYN;
        }

        inline bool RequiresAck() const {
            // The FIN requires an ACK as well but is not important for flow
            // performance (thus we don't analyze them)
            return data_len() || IsSyn();
        }

        // Returns TRUE, if this packet is marked as a retransmission but we
        // could not figure out its type (i.e. we don't know what caused it)
        inline bool MissesTrigger() const {
            return is_rtx() &&
                !(is_fast_rtx() || is_rto_rtx() || is_tlp() || is_spurious_rtx());
        }

        std::string FlagsAsString() const;

        void SetRelativeSeqAndAck(uint32_t seq_init, uint32_t ack_init);

        void Cut(const uint32_t offset, const uint32_t data_len);
        
        // Returns TRUE if any of the SACK blocks covers the sequence range of
        // the given packet
        bool IsSacked(const TcpSacks& sacks) const;

    private:
        // Check for bogus conditions, i.e. packets with invalid header settings
        // (e.g. no data offset)
        bool CheckBogus(const u_char* packet);

        // Parse some basic header options we might need later (e.g. timestamps,
        // MSS) if we captured the full options block
        void ParseOptions(const u_char* packet);

        const u_char* packet_;
        size_t len_;
        size_t caplen_;

        struct tcphdr* header_;
        
        uint32_t relative_seq_ = 0;
        uint32_t relative_ack_ = 0;

        const Packet* ack_packet_ = nullptr;

        // Most recent ACK received from the other endpoint before transmitting
        // this packet
        const Packet* last_ack_ = nullptr;
        
        TcpSacks sacks_;

        uint16_t mss_opt_value_ = 0;
        bool timestamp_ok_ = false;
        bool is_bogus_ = false;
        bool is_dupack_ = false;

        // If the header options are truncated we might not see some option(s).
        // This captures the bytes about which we don't know anything (i.e. not
        // even the option kind)
        uint16_t unknown_option_size_ = 0;

        // TRUE, if this packt is a spurious retransmission
        bool is_spurious_rtx_ = false;

        // TRUE, if this packet is a retransmission. Other fields below specify
        // the retransmission type more precisely. If all of them are false we
        // could not identify the trigger (i.e. what caused) this retransmission
        bool is_rtx_ = false;

        // TRUE, if this packet is a fast retransmission (or forward, or early
        // retransmission). At this point we don't care about distinguishing
        // between them
        bool is_fast_rtx_ = false;

        // TRUE, if this packet was triggered by an RTO firing
        bool is_rto_rtx_ = false;

        // TRUE, if this packet was triggered by an ACK while recovering from an
        // RTO
        bool is_slow_start_rtx_ = false;

        // TRUE, if this packet is a tail loss probe (TLP), i.e. was triggered
        // by the TLP timer
        bool is_tlp_ = false;

        // Time (in microseconds) between this packet's timestamp and the
        // corresponding ACK (this does not consider preliminary ACKing by a
        // SACK block)
        uint32_t ack_delay_us_ = 0;

        // Number of unacked bytes including the payload of this packet when
        // this packet was transmitted (this does not consider preliminary
        // ACKing by a SACK block)
        uint32_t unacked_bytes_ = 0;
        
        // Number of sent bytes (in-order) acked when this packet was
        // transmitted. We don't simply use the difference between initial and
        // current sequence numbers to protect against wraparound
        uint64_t acked_bytes_ = 0;

        // Time (in microseconds) between this packet's timestamp and the next
        // retransmission. Only valid if this packet has a retransmission
        uint32_t rtx_delay_us_ = 0;

        // Time (in microseconds) between this packet's timestamp and the
        // FINAL retransmission. Only valid if this packet is NOT already a
        // retransmission itself (i.e. this is only set for the original
        // transmission)
        uint32_t final_rtx_delay_us_ = 0;

        // Number of retransmission attempts. Only valid if this packet is
        // NOT already a retransmission itself (i.e. this is only set for
        // the original transmission)
        uint16_t num_rtx_attempts_ = 0;

        // If this packet is a RTO-induced retransmission or a TLP this field
        // carries information about the timer triggering this packet
        TcpTimerInfo rto_info_ = {nullptr, 0, 0, false};
        TcpTimerInfo tlp_info_ = {nullptr, 0, 0, false};

        // Estimated timer values when this packet was transmitted (assuming
        // that the timer would be armed now without any backoff)
        uint32_t rto_estimate_us_ = 0;
        uint32_t tlp_estimate_us_ = 0;
        uint32_t tlp_delayed_ack_estimate_us_ = 0;

        friend class TcpEndpoint;
        friend class TcpFlow;
};

#endif  /* TCP_PACKET_H_ */
