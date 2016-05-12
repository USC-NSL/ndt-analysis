#include "packet.h"

#include <cstring>
#include <stdio.h>
#include <stdlib.h>

#include "ethernet_packet.h"
#include "ip_packet.h"
#include "tcp_packet.h"

Packet::Packet(const u_char* packet, const struct pcap_pkthdr* pcap_header)
        : packet_(nullptr),
          ethernet_(nullptr),
          caplen_(pcap_header->caplen) {
    packet_ = std::make_unique<u_char[]>(caplen_);
    memcpy((void*) packet_.get(), (void*) packet, caplen_);

    ethernet_ = std::make_unique<EthernetPacket>(packet_.get(), caplen_);
    timestamp_us_ = pcap_header->ts.tv_sec * 1E6 + pcap_header->ts.tv_usec;
}

Packet::Packet(const Packet& packet)
        : packet_(nullptr),
          ethernet_(nullptr),
          caplen_(packet.caplen_) {
    packet_ = std::make_unique<u_char[]>(caplen_);
    memcpy((void*) packet_.get(), (void*) packet.packet_.get(), caplen_);

    ethernet_ = std::make_unique<EthernetPacket>(packet_.get(), caplen_);

    timestamp_us_ = packet.timestamp_us_;
}

Packet* Packet::CopyAndCut(const uint32_t offset,
        const uint32_t data_len) const {
    Packet* new_packet = new Packet(*this); 
    new_packet->ethernet_->Cut(offset, data_len);
    return new_packet;
}

EthernetPacket* Packet::ethernet() const {
    return ethernet_.get();
}

IpPacket* Packet::ip() const {
    return ethernet_->ip();
}

TcpPacket* Packet::tcp() const {
    if (ethernet_->ip() != nullptr) {
        return ethernet_->ip()->tcp();
    } else {
        return nullptr;
    }
}

bool Packet::IsLost() const {
    return rtx_ != nullptr && !rtx_->tcp()->is_spurious_rtx();
}

bool Packet::IsFromSameEndpoint(const Packet& packet) const {
    return ip()->src_addr() == packet.ip()->src_addr() &&
        tcp()->src_port() == packet.tcp()->src_port();
}
