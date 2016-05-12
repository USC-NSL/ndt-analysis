#include "tcp_flow.h"

#include <iostream>
#include <vector>

#include "ip_packet.h"
#include "tcp_endpoint.h"
#include "tcp_packet.h"

TcpFlow::TcpFlow(const TcpFlowId& id)
        : id_(id), endpoint_a_(nullptr), endpoint_b_(nullptr) {}

bool TcpFlow::AddPacket(std::unique_ptr<Packet> packet, bool process_packet) {
    auto packet_ptr = packet.get();
    owned_packets_.push_back(std::move(packet));
    return AddPacketToEndpoint(packet_ptr, process_packet);
}

bool TcpFlow::AddPacket(Packet* packet, bool process_packet) {
    packets_.push_back(packet);
    return AddPacketToEndpoint(packet, process_packet);
}

bool TcpFlow::AddPacketToEndpoint(Packet* packet, bool process_packet) {
    TcpEndpoint* current_sender;
    TcpEndpoint* current_receiver;

    if (endpoint_a_ == nullptr) {
        // This is the first packet for this flow, therefore create the first
        // endpoint
        endpoint_a_ = std::make_unique<TcpEndpoint>(packet);
    }
    
    // Check if the packet carries the MSS option which we might have to buffer
    // for the other endpoint to use later
    if (process_packet) {
        CheckForMSS(*packet);
    }
    
    if (packet->ip()->src_addr() == endpoint_a_->addr_ &&
        packet->tcp()->src_port() == endpoint_a_->port_) {
        current_sender = endpoint_a();
        current_receiver = endpoint_b();
    } else {
        // Packet comes from the other endpoint (B), generate this endpoint if
        // it does not exist yet. At this point we should also have extracted
        // the MSS value from the respective header option
        if (endpoint_b_ == nullptr) {
            endpoint_b_ = std::make_unique<TcpEndpoint>(packet);
            if (mss_a_) {
                endpoint_a_->mss_ = mss_a_;
            }
            if (mss_b_) {
                endpoint_b_->mss_ = mss_b_;
            }
        }
        current_sender = endpoint_b();
        current_receiver = endpoint_a();
    }
    current_sender->AddPacket(packet, process_packet);

    // Process ACKs for the opposite endpoint
    if (process_packet && packet->tcp()->IsAck() && current_receiver != nullptr) {
        current_receiver->ProcessAck(packet);
    }

    return !current_sender->is_bogus();
}

void TcpFlow::CheckForMSS(const Packet& packet) {
    const TcpPacket& tcp = *(packet.tcp());
    if (!tcp.IsSyn()) {
        return;
    }
    
    // Extract the advertised MSS from this handshake packet
    uint32_t mss_opt_value = tcp.mss_opt_value();
    if (!mss_opt_value) {
        return;
    }
    // If timestamps are enabled, the MSS (usable for payload) is further
    // reduced (since timestamps count towards the maximum)
    if (tcp.timestamp_ok()) {
        mss_opt_value -= 12;
    }

    if (packet.ip()->src_addr() == endpoint_a_->addr_ &&
        packet.tcp()->src_port() == endpoint_a_->port_) {
        // Packet comes from endpoint A, therefore this is the MSS usable by
        // endpoint B (and vice versa)
        mss_b_ = mss_opt_value;
    } else {
        mss_a_ = mss_opt_value;
    }
}

std::vector<std::unique_ptr<TcpFlow>> TcpFlow::SplitIntoSegments() const {
    std::vector<std::unique_ptr<TcpFlow>> segments;
    
    if (packets_.empty()) {
        return segments;
    }

    const TcpEndpoint* current_sender = endpoint_a_.get();
    TcpFlow* current_segment = new TcpFlow(id_);
    segments.push_back(std::unique_ptr<TcpFlow>(current_segment));

    for (auto& packet : owned_packets_) {
        // Non-data packets do not trigger a new segment
        if (!packet->tcp()->data_len()) {
            current_segment->AddPacket(packet.get(), false);
            continue;
        }

        // If data comes from the other endpoint: enter response phase or create
        // a new segment
        const IpPacket& ip = *(packet->ip());
        if (current_sender->addr_ != ip.src_addr() ||
            current_sender->port_ != ip.tcp()->src_port()) {
            if (current_sender == endpoint_a_.get()) {
                current_sender = endpoint_b_.get();
            } else {
                current_sender = endpoint_a_.get();
                current_segment = new TcpFlow(id_);
                segments.push_back(std::unique_ptr<TcpFlow>(current_segment));
            }
        }
        current_segment->AddPacket(packet.get(), false);
    }

    return segments;
}
