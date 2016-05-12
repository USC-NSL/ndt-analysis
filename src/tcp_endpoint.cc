#include "tcp_endpoint.h"
#include <iostream>
#include <algorithm>
#include <complex>

#include <glog/logging.h>

#include "ip_packet.h"
#include "tcp_packet.h"
#include "util.h"

constexpr uint64_t TcpEndpoint::kMaxTriggerPacketDelayUs = 2000;

TcpEndpoint::TcpEndpoint(Packet* packet)
        : addr_(packet->ip()->src_addr()),
          port_(packet->tcp()->src_port()) {
    current_packet_ = packet;
    SetInitialSequenceNumbers();        
}

void TcpEndpoint::SetInitialSequenceNumbers() {
    const TcpPacket& tcp = *(current_packet_->tcp());
    const bool ack_flag_set = tcp.flags() & TH_ACK;

    if (!seq_init_) {
        seq_acked_ = seq_init_ = tcp.seq();
        seq_next_ = seq_acked_ + 1;
    }
    if (!ack_init_ && ack_flag_set) {
        ack_ = tcp.ack();
        ack_init_ = ack_ - 1;
    }
    if (seq_init_ && ack_init_) {
        seq_initialized_ = true;
    }
}

std::vector<Packet*> TcpEndpoint::AddPacket(Packet* packet, bool process_packet) {
    current_packet_ = packet;
    if (!seq_initialized_) {
        SetInitialSequenceNumbers();
    }

    // Determine the maximum segment size (MSS) so that we can split coalesced
    // packets properly into the (likely) on-the-wire packets
    if (process_packet && !mss_) {
        DeriveMSS();
    }
    
    std::vector<Packet*> wire_packets;
    if (process_packet) {
        wire_packets = SplitIntoWirePackets();
    } else {
        wire_packets.push_back(current_packet_);
    }

    for (Packet* wire_packet : wire_packets) {
        if (!packets_.empty()) {
            auto* previous_packet = packets_.back();
            wire_packet->set_previous_packet(previous_packet);
            previous_packet->set_next_packet(wire_packet);
        }
        current_packet_ = wire_packet;
        TcpPacket* wire_tcp = wire_packet->tcp();
        wire_tcp->SetRelativeSeqAndAck(seq_init_, ack_init_);
        wire_tcp->acked_bytes_ = acked_bytes_;

        // Update state for packets requiring ACKs, i.e. data packets and
        // SYN/FIN packets (we capture packets at the
        // server side, so we only worry about packets being potentially
        // retransmissions here)
        if (process_packet && wire_tcp->RequiresAck()) {
            if (wire_tcp->data_len()) {
                bool seq_moved = false;
                if (tcp_util::After(wire_tcp->seq_end(), seq_next_)) {
                    seq_next_ = wire_tcp->seq_end();
                    seq_moved = true;
                }
                if (rto_high_seq_ && tcp_util::After(seq_next_, rto_high_seq_)) {
                    rto_high_seq_ = 0;
                }
                if (!seq_moved || rto_high_seq_) {
                    // Sequence was transmitted before (i.e. retransmission)
                    ProcessRtx();
                }
                wire_tcp->unacked_bytes_ = GetUnackedBytes();
                wire_tcp->last_ack_ = last_ack_;
                wire_tcp->rto_estimate_us_ = timer_.GetRTO();
                wire_tcp->tlp_estimate_us_ = timer_.GetTLP(false);
                wire_tcp->tlp_delayed_ack_estimate_us_ = timer_.GetTLP(true);
                num_data_packets_ += 1;
            } else if (wire_tcp->IsSyn()) {
                if (!unacked_packets_.empty()) {
                    // This is not the first packet (SYN)
                    ProcessRtx();
                }
            }

            unacked_packets_.push_back(wire_packet);
            if (rto_.armed_by_ == nullptr) {
                ArmTimers(current_packet_);
            }
        }
        packets_.push_back(wire_packet);
    }

    return wire_packets;
}

void TcpEndpoint::ArmTimers(const Packet* packet) {
    rto_.armed_by_ = packet;
    rto_.delay_us_ = timer_.GetRTO(num_rtos_);
    rto_.backoffs_ = num_rtos_;
    tlp_.armed_by_ = packet;

    bool delayed_ack = false;
    for (Packet* packet : unacked_packets_) {
        if (!packet->IsLost()) {
            if (delayed_ack) {
                delayed_ack = false;
                break;
            } else {
                delayed_ack = true;
            }
        }
    }
    tlp_.delay_us_ = timer_.GetTLP(delayed_ack);
    tlp_.delayed_ack_ = delayed_ack;
}

void TcpEndpoint::DeriveMSS() {
    const TcpPacket& tcp = *(current_packet_->tcp());

    // Estimate the MSS 
    const uint32_t data_len = tcp.data_len();
    constexpr uint32_t kMinMSS = 536;  // based on RFC 1122
    constexpr uint32_t kMaxMSS = 1460; // based on Ethernet v2

    if (data_len < kMinMSS) {
        // Packet carries a payload less than the minimum possible
        return;
    } else if (data_len <= kMaxMSS) {
        // Check if the packet carries a single MSS (we assume that this is
        // the case if the packet is at most as large as an Ethernet frame)
        mss_ = data_len;
    } else {
        // A large frame can carry up to 10 MSS payload. Find a split that
        // gives us non-fractional payload size per packet.
        for (uint16_t mult = 2; mult <= 10; mult++) {
            if (data_len % mult == 0 && data_len / mult <= kMaxMSS) {
                mss_ = data_len / mult;
                break;
            }
        }
    }
}

std::vector<Packet*> TcpEndpoint::SplitIntoWirePackets() {
    std::vector<Packet*> wire_packets;
    uint32_t data_len = current_packet_->tcp()->data_len();
    
    if (mss_ == 0 || data_len <= mss_) {
        wire_packets.push_back(current_packet_);
        return wire_packets;
    }

    // Split the payload across multiple packets carrying at most MSS bytes
    // each. Except sequence numbers and data length all other fields are
    // copied.
    uint32_t offset = 0;
    while (offset < data_len) {
        uint32_t current_data_len = std::min(mss_, data_len - offset);
        Packet* new_packet =
            current_packet_->CopyAndCut(offset, current_data_len);
        wire_packets.push_back(new_packet);
        offset += current_data_len;
    }

    return wire_packets;
}

void TcpEndpoint::AdjustUnackedBytesCountsAfter(
        const Packet& packet, int32_t offset) {
    for (auto it = packets_.rbegin(); it != packets_.rend(); ++it) {
        Packet* current_packet = *it;
        if (current_packet == &packet) {
            return;
        }
        current_packet->tcp()->unacked_bytes_ += offset;
    }
}

bool TcpEndpoint::TiePacketToSackLookalike(Packet* packet) {
    while (!unused_sack_lookalikes_.empty()) {
        // We consume the oldest lookalike, as long as the lookalike was
        // received at least min RTT after the packet was sent, and
        auto possible_sack = unused_sack_lookalikes_.front();
        unused_sack_lookalikes_.pop();
        if (packet->timestamp_us() + min_rtt_us_ < possible_sack->timestamp_us()) {
            VLOG(3) << "Seq " << packet->tcp()->seq()
                    << ": tied to SACK lookalike (ack: " << possible_sack->tcp()->ack() << ")";
            HandleAckedPacket(packet, possible_sack);
            sacks_.Add({packet->tcp()->seq(), packet->tcp()->seq_end()});
            if (!last_ack_with_trigger_ ||
                    possible_sack->timestamp_us() >
                    last_ack_with_trigger_->timestamp_us()) {
                last_ack_with_trigger_ = possible_sack;
                ArmTimers(possible_sack);

                // In addition all unacked_bytes counts for packets seen after
                // this SACK are now off and need to be corrected
                AdjustUnackedBytesCountsAfter(
                        *possible_sack, packet->tcp()->data_len());
            }
            return true;
        }
    }
    return false;
}

void TcpEndpoint::CheckForSacksFromLookalikes() {
    if (unused_sack_lookalikes_.empty()) {
        return;
    }

    // All packets that are older than the currently retransmitted packet that
    // have not been retransmitted yet were likely sacked and we tie each of
    // them to the next unused SACK lookalike
    for (auto it = unacked_packets_.begin();
         it != unacked_packets_.end() && !unused_sack_lookalikes_.empty(); ) {
        Packet* unacked_packet = *it;
        TcpPacket* tcp = unacked_packet->tcp();
        if (!tcp_util::After(current_packet_->tcp()->seq(), tcp->seq())) {
            return;
        } else if (!unacked_packet->IsLost() &&
                   unacked_packet->previous_tx() == nullptr) {
            if (!TiePacketToSackLookalike(unacked_packet)) {
                return;
            }
            it = unacked_packets_.erase(it);
        } else {
            ++it;
        }
    }
}

void TcpEndpoint::ProcessRtx() {
    CheckForSacksFromLookalikes();
    current_packet_->tcp()->is_rtx_ = true;

    if (!FindRtxTrigger()) {
        VLOG(3) << "Seq " << current_packet_->tcp()->seq()
                << ": Could not determine retransmission trigger";
    }
    if (!LinkToPreviousTx()) {
        unmatched_rtx_++;
        if (unmatched_rtx_ > 100) {
            // There is certainly something fishy if we cannot map that many
            // packets to earlier transmissions. We are aborting here since
            // dealing with these cases is very CPU-intensive.
            is_bogus_ = true;
            VLOG(1) << "Too many unmatched retransmissions. Marking as bogus";
        }
        VLOG(1) << "Seq " << current_packet_->tcp()->seq()
                << ": Did not find earlier transmission for a retransmission "
                << "(unmatched count: " << unmatched_rtx_ << ")";
    }
    MarkPacketsOutOfOrder();
}

bool TcpEndpoint::CheckForTLP() {
    if (tlp_.armed_by_ == nullptr || !is_tlp_enabled_) {
        return false;
    }
    const Packet* armer = tlp_.armed_by_;

    // The TLP carries at least the highest previously transmitted sequence
    // (this is true, since we are only getting here for retransmitted packets,
    // i.e. the cases where the TLP carries new data are not covered here)
    if (current_packet_->tcp()->seq_end() != seq_next_) {
        return false;
    }

    // Compare this packet's timestamp with the estimated timestamp for
    // the tail loss probe
    uint64_t time_diff_us =
        std::abs(static_cast<int64_t>(tlp_.fire_us() -
                 static_cast<int64_t>(current_packet_->timestamp_us())));

    // TODO what to use as cutoff?
    if (time_diff_us > 0.2 * tlp_.delay_us_) {
        return false;
    }

    // At this point we treat this retransmission as a TLP
    tlp_.delay_us_ = current_packet_->timestamp_us() - armer->timestamp_us();
    current_packet_->tcp()->is_tlp_ = true;
    current_packet_->tcp()->tlp_info_ = tlp_;
    
    return true;
}

bool TcpEndpoint::CheckForRTO() {
    if (rto_.armed_by_ == nullptr) {
        return false;
    }
    const Packet* armer = rto_.armed_by_;

    // Compare this packet's timestamp with the estimated timestamp for the RTO
    // event. Since different stacks compute the RTO differently we only require
    // that the timestamps are close to each other
    uint64_t time_diff_us =
        std::abs(static_cast<int64_t>(rto_.fire_us() -
                 static_cast<int64_t>(current_packet_->timestamp_us())));

    VLOG(3) << "Seq " << current_packet_->tcp()->seq()
            << ": delay: "
            << current_packet_->timestamp_us() - armer->timestamp_us()
            << ", estimated RTO: "
            << rto_.fire_us() - armer->timestamp_us()
            << ", base RTO: "
            << timer_.GetRTO(0);

    // TODO what to use as cutoff?
    if (time_diff_us > 0.2 * rto_.delay_us_) {
        // If the previous tx could also have been a TLP, check if this
        // retransmission could be caused by an RTO without backoff. In that
        // case the previous retransmission was caused by TLP and not an RTO
        if (is_tlp_enabled_ && num_rtos_ == 1 && armer->tcp()->is_tlp()) {
            num_rtos_--;
            rto_high_seq_ = 0;
            armer->tcp()->is_rto_rtx_ = false;
            ArmTimers(armer);
            return CheckForRTO();
        } else {
            return false;
        }
    }
    rto_.delay_us_ = current_packet_->timestamp_us() - armer->timestamp_us();

    // At this point we tie this retransmission to an RTO and update the timer
    current_packet_->tcp()->is_rto_rtx_ = true;
    current_packet_->tcp()->rto_info_ = rto_;
    num_rtos_++;
    rto_high_seq_ = seq_next_;

    // If this is the first retransmission and it does not match the TLP
    // conditions as well, then TLP is not enabled for this connection
    if (num_rtos_ == 1 && !current_packet_->tcp()->is_tlp()) {
        is_tlp_enabled_ = false;
    }

    // If this is the second retransmission and we already have backed off, then
    // the first retransmission was not caused by a TLP
    if (num_rtos_ == 2 && armer->tcp()->is_tlp()) {
        is_tlp_enabled_ = false;
        armer->tcp()->is_tlp_ = false;
    }
    
    return true;
}

bool TcpEndpoint::FindRtxTrigger() {
    if (last_ack_ != nullptr) {
        // The current packet is considered the result of an incoming ACK
        // if there is a very short delay between them
        const uint64_t elapsed_time_us =
            current_packet_->timestamp_us() - last_ack_->timestamp_us();
        if (elapsed_time_us <= kMaxTriggerPacketDelayUs) {
            // The trigger packet for the retransmission is same trigger as for
            // the last ACK packet
            // (i.e. trigger data -> trigger ACK -> retransmission)
            // Since in some circumstances we might not be able to know the
            // trigger packet (e.g. when SACK blocks are not captured) we use
            // the next closest ACK for which we have a trigger packet.
            VLOG(3) << "Seq " << current_packet_->tcp()->seq()
                    << ": Incoming packet triggered retransmission";
            if (last_ack_with_trigger_) {
                current_packet_->set_trigger_packet(
                        last_ack_with_trigger_->trigger_packet());
            }

            // If the high_seq is set we are currently in recovery mode, and
            // this is a slow-start retransmission. Otherwise, it's a fast
            // retransmission
            if (rto_high_seq_) {
                current_packet_->tcp()->is_slow_start_rtx_ = true;
            } else {
                current_packet_->tcp()->is_fast_rtx_ = true;
            }
            return true;
        }
    }

    // Next check for TLPs and RTO-induced retransmissions
    const bool has_tlp = CheckForTLP();
    const bool has_rto = CheckForRTO();
    if (has_tlp || has_rto) {
        VLOG(3) << "Seq " << current_packet_->tcp()->seq()
                << ": Timeout triggered retransmission";
        ArmTimers(current_packet_);
        tlp_.clear();
        return true;
    } else {
        return false;
    }
}

bool TcpEndpoint::LinkToPreviousTx() {
    const TcpPacket& tcp = *(current_packet_->tcp());
    
    // Search from the back for the most recent transmission that covers at the
    // least the starting sequence of the current packet
    for (auto rev_iter = packets_.rbegin();
            rev_iter != packets_.rend(); ++rev_iter) {
        Packet* previous_packet = *rev_iter;
        const TcpPacket& previous_tcp = *(previous_packet->tcp());
        
        if (tcp.seq() == previous_tcp.seq() ||
            tcp_util::Between(tcp.seq(), previous_tcp.seq(),
                previous_tcp.seq_end())) {
            // Found earlier transmission
            previous_packet->set_rtx(current_packet_);
            current_packet_->set_previous_tx(previous_packet);
            current_packet_->set_first_tx(previous_packet->first_tx());

            // Set retransmission delays and attempt counts for the
            // previous matching transmissions
            Packet* current_tx = current_packet_;
            while (current_tx->previous_tx() != nullptr) {
                current_tx = current_tx->previous_tx();
                TcpPacket* current_tcp = current_tx->tcp();
                const uint32_t delay =
                    current_packet_->timestamp_us() - current_tx->timestamp_us();
                if (!current_tcp->rtx_delay_us_) {
                    current_tcp->rtx_delay_us_ = delay;
                }

                // If we moved to the original transmission set total
                // delay and number of attempts as well
                if (current_tx->previous_tx() == nullptr) {
                    current_tcp->final_rtx_delay_us_ = delay;
                    current_tcp->num_rtx_attempts_++;
                }
            }
            return true;
        }
    }

    // If we get here we have not found any earlier packet that covered
    // at least the starting sequence
    return false;
}

void TcpEndpoint::MarkPacketsOutOfOrder() {
    const Packet* previous_tx = current_packet_->previous_tx();
    Packet* packet = current_packet_->previous_packet();

    while (packet != previous_tx) {
        if (packet->tcp()->data_len()) {
            packet->set_out_of_order(true);
        }
        packet = packet->previous_packet();
    }
}

void TcpEndpoint::ProcessAck(Packet* packet) {
    current_packet_ = last_ack_ = packet;
    TcpPacket* tcp = current_packet_->tcp();

    // If the ACK number advanced or if the packet carried SACK blocks we check
    // if the unacked packets are now fully acked
    bool ack_moved = tcp_util::After(tcp->ack(), seq_acked_);
    bool has_sacks = !tcp->sacks().empty();
    if (ack_moved) {
        acked_bytes_ += tcp->ack() - seq_acked_;
        seq_acked_ = tcp->ack();
        if (tcp_util::After(seq_acked_, seq_next_)) {
            VLOG(1) << "ACK for seq after seq_next (acked: " << seq_acked_
                    << ", next: " << seq_next_ << ")";
            is_bogus_ = true;
        }
        if (rto_high_seq_ && !tcp_util::Before(seq_acked_, rto_high_seq_)) {
            rto_high_seq_ = 0;
        }
    } else if (tcp->ack() == seq_acked_) {
        tcp->is_dupack_ = true;
        if (tcp->unknown_option_size() >= 10) {
            // Options are truncated but at least one SACK would fit in here. We
            // treat this as a SACK lookalike and use it the retroactively ack
            // packets if we later find out that newer packets are retransmitted
            // before older packets (indicating that the older ones were sacked)
            unused_sack_lookalikes_.push(packet);
        }
    }
    if (ack_moved || has_sacks) {
        AckPackets();
        sacks_.Add(tcp->sacks());
        sacks_.RemoveAcked(seq_acked_);
        num_rtos_ = 0;
    }
    if (has_sacks) {
        DSackPackets();
    }
}

void TcpEndpoint::HandleAckedPacket(Packet* packet, Packet* ack) {
    TcpPacket* tcp = packet->tcp();
    tcp->ack_packet_ = ack;
    tcp->ack_delay_us_ = ack->timestamp_us() - packet->timestamp_us();
    if (ack->timestamp_us() < packet->timestamp_us()) {
        VLOG(3) << "ACK with timestamp earlier than data ("
                << (packet->timestamp_us() - ack->timestamp_us()) / 1000
                << "ms earlier)";
    }
    // Update our min-RTT guess. We exclude ACKs for retransmissions since with
    // truncated packets we cannot see DSACKs (for spurious retransmissions)
    // which can look like RTTs being close to zero
    if (packet->previous_tx() == nullptr &&
            (tcp->ack_delay_us_ < min_rtt_us_ || !min_rtt_us_)) {
        min_rtt_us_ = tcp->ack_delay_us_;
    }

    // Set the trigger packet (i.e. the data packet that resulted in the
    // transmission of this ACK)
    ack->set_trigger_packet(packet);

    // If the ACKed packet is NOT a retransmission or related to a lost
    // sequence we use the RTT sample to update the RTT estimate
    // (used for timers)
    if (packet->previous_tx() == nullptr && packet->rtx() == nullptr) {
        timer_.AddSample(packet, seq_acked_, seq_next_);
    }
}

void TcpEndpoint::AckPackets() {
    std::vector<Packet*> remaining_unacked_packets;
    const TcpSacks& sacks = current_packet_->tcp()->sacks();
    bool acked_data = false;

    for (Packet* unacked_packet : unacked_packets_) {
        TcpPacket* tcp = unacked_packet->tcp();
        if (!tcp_util::After(tcp->seq_end(), seq_acked_) ||
            tcp->IsSacked(sacks)) {
            // Packet is ACKed (or SACKed)
            acked_data = true;
            HandleAckedPacket(unacked_packet, last_ack_);
        } else {
            remaining_unacked_packets.push_back(unacked_packet);
        }
    }
    unacked_packets_ = remaining_unacked_packets;

    // Reset the TLP and RTO timer if new data was ACKed,
    // turn off the RTO timer if there is no more pending data
    if (acked_data) {
        ArmTimers(last_ack_);
        last_ack_with_trigger_ = last_ack_;
    }
    if (unacked_packets_.empty()) {
        rto_.clear();
        tlp_.clear();
    }
}

void TcpEndpoint::DSackPackets() {
    uint32_t ack = current_packet_->tcp()->ack();
    const auto sacks = current_packet_->tcp()->sacks().sacks();
    for (const Sack sack : sacks) {
        if (tcp_util::Before(sack.start_, ack) &&
                !tcp_util::After(sack.end_, ack)) {
            // DSACK range
            HandleSpuriousRtx(sack.start_, sack.end_);
        }
    }
}

void TcpEndpoint::HandleSpuriousRtx(uint32_t seq_start, uint32_t seq_end) {
   for (auto rev_iter = packets_.rbegin();
           rev_iter != packets_.rend(); ++rev_iter) {
        Packet* packet = *rev_iter;
        // Find the latest packet that was retransmitted and does not have
        // a spurious retransmission attached to it already. That is, the
        // DSACK marks the last retransmission as spurious, the second
        // DSACK marks the second last retransmission as spurious, etc.
        if (packet->tcp()->is_rtx() &&
                !packet->tcp()->is_spurious_rtx() &&
                tcp_util::RangeIncluded(
                    seq_start, seq_end,
                    packet->tcp()->seq(),
                    packet->tcp()->seq_end())) {
            packet->tcp()->is_spurious_rtx_ = true;
            return;
        }
   }
}

uint32_t TcpEndpoint::GetNumLosses() const {
    return GetCountForNonZeroFunctionValue(&Packet::IsLost);
}

uint32_t TcpEndpoint::GetNumDataPackets() const {
    return GetCountForNonZeroFunctionValue(&TcpPacket::data_len);
}

uint32_t TcpEndpoint::GetNumSackPackets() const {
    return GetCountForNonZeroFunctionValue(&TcpPacket::has_sack);
}

uint32_t TcpEndpoint::GetNumMissingTriggerPackets() const {
    return GetCountForNonZeroFunctionValue(&TcpPacket::MissesTrigger);
}

std::vector<uint32_t> TcpEndpoint::GetRttsUs() const {
    std::vector<uint32_t> rtts;
    for (const Packet* packet : packets_) {
        if (!packet->IsLost() && !packet->out_of_order() &&
                packet->tcp()->ack_delay_us()) {
            rtts.push_back(packet->tcp()->ack_delay_us());
        }
    }
    return rtts;
}

void TcpEndpoint::SetPassedBytesForPackets() {
    uint64_t num_bytes = 0;
    for (Packet* packet : packets_) {
        packet->set_bytes_passed(num_bytes);
        if (!packet->IsLost()) {
            num_bytes += packet->tcp()->data_len();
        }
    }
}

uint64_t TcpEndpoint::GetGoodputBps(bool cut_off_at_loss) const {
    uint64_t bytes_acked = 0;
    uint64_t first_data_timestamp_us = 0;
    uint64_t last_ack_timestamp_us = 0;

    for (const Packet* packet : packets_) {
        if (!packet->tcp()->data_len()) {
            continue;
        }
        if (!first_data_timestamp_us) {
            first_data_timestamp_us = packet->timestamp_us();
        }
        if (packet->IsLost()) {
            if (cut_off_at_loss) {
                break;
            } else {
                continue;
            }
        }
        last_ack_timestamp_us = std::max(last_ack_timestamp_us,
                packet->timestamp_us() + packet->tcp()->ack_delay_us());
        bytes_acked += packet->tcp()->data_len();
    }

    if (!first_data_timestamp_us || !last_ack_timestamp_us ||
            first_data_timestamp_us == last_ack_timestamp_us) {
        return 0;
    }

    const uint64_t time_elapsed_us = last_ack_timestamp_us - first_data_timestamp_us;
    return bytes_acked * 8E6 / time_elapsed_us;
}
std::vector<std::pair<double, double>>
        TcpEndpoint::GetUnackedBytesRttPairs() const {
    std::vector<std::pair<double, double>> pairs;
    for (const Packet* packet : packets_) {
        if (!packet->IsLost() && !packet->out_of_order() &&
                packet->tcp()->ack_delay_us()) {
            pairs.push_back(std::make_pair(
                    packet->tcp()->unacked_bytes(),
                    packet->tcp()->ack_delay_us()));
        }
    }
    return pairs;
}

std::vector<std::pair<double, double>>
        TcpEndpoint::GetUnackedBytesRttPairsAroundPacket(
                const Packet& target_packet, uint8_t num_samples,
                bool use_older_packets_only) const {
    const uint8_t max_distance =
        use_older_packets_only ? num_samples : (num_samples / 2);
    std::vector<std::pair<double, double>> pairs;
    std::vector<std::pair<double, double>> before_pairs(max_distance);
    bool seen = false;
    int index = 0;

    // Fill up a circular buffer with packets preceding the target
    auto packet_it = packets_.cbegin();
    for (;packet_it != packets_.cend(); ++packet_it) {
        const Packet* packet = *packet_it;
        if (!packet->IsLost() && !packet->out_of_order() &&
                packet->tcp()->ack_delay_us()) {
            before_pairs[index % max_distance] = 
                std::make_pair(
                        packet->tcp()->unacked_bytes(),
                        packet->tcp()->ack_delay_us());
            index++;
        }
        if (packet == &target_packet) {
            seen = true;
            break;
        }
    }
    if (!seen || !index) {
        return pairs;
    }

    int end_index = index - 1;
    index = std::max(0, index - max_distance);
    for (int i = index; i <= end_index; i++) {
        pairs.push_back(before_pairs[i % max_distance]);
    }
    if (use_older_packets_only) {
        return pairs;
    }

    // Found the target packet. Now get the immediately succeeding samples until
    // hitting the max. distance
    int seen_after = 0;
    for (; packet_it != packets_.cend() && seen_after < max_distance; ++packet_it) {
        const Packet* packet = *packet_it;
        if (!packet->IsLost() && !packet->out_of_order() &&
                packet->tcp()->ack_delay_us()) {
            pairs.push_back(std::make_pair(
                    packet->tcp()->unacked_bytes(),
                    packet->tcp()->ack_delay_us()));
            seen_after++;
        }
    }

    return pairs;
}
