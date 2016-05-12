#include "ip_packet.h"

IpPacket::IpPacket(u_char* packet, const uint32_t caplen)
        : packet_(packet),
          header_(nullptr),
          tcp_(nullptr) {
    size_t header_len = sizeof(struct ip);
    if (caplen < header_len) {
        return;
    }

    header_ = (struct ip*) packet;
    if (header_->ip_p == IPPROTO_TCP) {
        u_char* tcp_packet = packet + header_len;
        int32_t tcp_caplen = caplen - header_len;
        if (tcp_caplen > 0) {
            tcp_ = std::make_unique<TcpPacket>(tcp_packet, data_len(), tcp_caplen);
        }
    }
}

void IpPacket::Cut(const uint32_t offset, const uint32_t data_len) {
    tcp_->Cut(offset, data_len);
}
