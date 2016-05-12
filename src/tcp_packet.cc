#include "tcp_packet.h"

#include <iostream>
#include <sstream>

#include "util.h"

TcpPacket::TcpPacket(u_char* packet, const uint32_t len, const uint32_t caplen)
        : packet_(packet),
          len_(len),
          caplen_(caplen),
          header_(nullptr) {
    const size_t header_len = sizeof(struct tcphdr);
    if (caplen < header_len) {
        return;
    }

    header_ = (struct tcphdr*) packet;

    // Check for bogus packets
    is_bogus_ = CheckBogus(packet);
    if (is_bogus_) {
        return;
    }

    // Parse options if we captured enough bytes
    ParseOptions(packet);
}

bool TcpPacket::CheckBogus(const u_char* packet) {
    // Illegal lengths/offsets
    const size_t header_len = sizeof(struct tcphdr);
    if (data_offset() < header_len ||
            data_offset() > len_) {
        std::cerr << "Illegal data offset (is: " << data_offset()
                  << ", header length: " << header_len
                  << ", packet length: " << len_ << ")" << std::endl;
        return true;
    }

    // Illegal ports, ACK number 0, SYN with payload
    if (!src_port() ||
            !dst_port()) {
        std::cerr << "Illegal port (is: 0)" << std::endl;
        return true;
    }
    if (IsAck() && !ack()) {
        std::cerr << "Illegal ACK number (is: 0)" << std::endl;
        return true;
    }
    if (flags() == TH_SYN && data_len()) {
        std::cerr << "Illegal packet structure: SYN with payload" << std::endl;
        return true;
    }

    // Illegal flag combinations (by checking if combination matches any of the
    // legal flag combinations)
    const uint8_t main_flags = flags() & ~(TH_PUSH|TH_URG|TH_ECE|TH_CWR);
    if (main_flags != TH_ACK &&
            main_flags != TH_SYN &&
            main_flags != (TH_SYN|TH_ACK) &&
            main_flags != (TH_FIN|TH_ACK) &&
            main_flags != TH_FIN &&
            main_flags != TH_RST &&
            main_flags != (TH_RST|TH_ACK)) {
        std::cerr << "Illegal header flag combination (is: "
                  << FlagsAsString() << ")" << std::endl;
        return true;
    }

    return false;
}

void TcpPacket::ParseOptions(const u_char* packet) {
    const size_t header_len = sizeof(struct tcphdr);
    const size_t opt_len = data_offset() - header_len;
    const size_t cap_opt_len = caplen_ - header_len;
    if (!opt_len || cap_opt_len < 2) {
        // We can't do anything here if we don't at least capture
        // option kind and size of the first option
        unknown_option_size_ = opt_len;
        return;
    }

    const u_char* options = packet + header_len;
    size_t used_bytes = 0;
    while (used_bytes < cap_opt_len) {
        const u_char* current_option = options + used_bytes;
        const uint8_t opt_kind = *current_option;
        const size_t remaining_bytes = cap_opt_len - used_bytes; 
        switch (opt_kind) {
            case TCPOPT_MAXSEG:
                if (remaining_bytes >= 4) {
                    mss_opt_value_ =
                        ntohs(*reinterpret_cast<const uint16_t*>(current_option+2));
                }
                break;
            case TCPOPT_TIMESTAMP:
                timestamp_ok_ = true;
                break;
            case TCPOPT_SACK:
                if (!sacks_.Parse(current_option, remaining_bytes)) {
                    is_bogus_ = true;
                    return;
                }
                break;
        }

        if (opt_kind == TCPOPT_NOP) {
            used_bytes++;
        } else if (used_bytes + 1 == cap_opt_len) {
            // Did not capture option length
            break;
        } else {
            const uint8_t opt_size = *(current_option+1);
            if (!opt_size || opt_size > opt_len) {
                is_bogus_ = true;
                return;
            }
            used_bytes += opt_size;
        }
    }
    if (used_bytes < opt_len) {
        unknown_option_size_ = opt_len - used_bytes;
    }
}

std::string TcpPacket::FlagsAsString() const {
    std::ostringstream buffer;
    if (flags() & TH_FIN) {
        buffer << "[FIN]";
    }
    if (flags() & TH_SYN) {
        buffer << "[SYN]";
    }
    if (flags() & TH_RST) {
        buffer << "[RST]";
    }
    if (flags() & TH_PUSH) {
        buffer << "[PSH]";
    }
    if (flags() & TH_ACK) {
        buffer << "[ACK]";
    }
    if (flags() & TH_URG) {
        buffer << "[URG]";
    }
    if (flags() & TH_ECE) {
        buffer << "[ECE]";
    }
    if (flags() & TH_CWR) {
        buffer << "[CWR]";
    }

    return buffer.str();
}

void TcpPacket::SetRelativeSeqAndAck(uint32_t seq_init, uint32_t ack_init) {
    relative_seq_ = seq() - seq_init;
    relative_ack_ = ack() - ack_init;
}

void TcpPacket::Cut(const uint32_t offset, const uint32_t data_len) {
    const size_t header_len = sizeof(struct tcphdr);
    const size_t opt_len = data_offset() - header_len;
    len_ = header_len + opt_len + data_len;

    // Fix the captured length based on the removed payload parts
    if (caplen_ > header_len + opt_len + offset) {
        caplen_ = std::min(len_,  caplen_ - offset);
    } else if (caplen_ > header_len + opt_len) {
        caplen_ = header_len + opt_len;
    }

    // Update sequence number
    header_->th_seq = htonl(ntohl(header_->th_seq) + offset);
}

bool TcpPacket::IsSacked(const TcpSacks& sacks) const {
    for (const Sack sack : sacks.sacks()) {
        if (tcp_util::RangeIncluded(
                    seq(), seq_end(), sack.start_, sack.end_)) {
            return true;
        }
    }
    return false;
}
