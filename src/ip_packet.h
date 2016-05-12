#ifndef IP_PACKET_H_
#define IP_PACKET_H_

#include "tcp_packet.h"

#include <arpa/inet.h>
#include <memory>
#include <netinet/ip.h>
#include <netinet/tcp.h>

class IpPacket {
    public:
        IpPacket(u_char* packet, const uint32_t caplen);

        inline const u_char* packet() const {
            return packet_;
        }
        inline const uint32_t src_addr() const {
            return header_->ip_src.s_addr;
        }
        inline const uint32_t dst_addr() const {
            return header_->ip_dst.s_addr;
        }
        inline TcpPacket* tcp() const {
            return tcp_.get();
        }
        inline const uint32_t header_len() const {
            return header_->ip_hl << 2;
        }
        inline const uint32_t data_len() const {
            return ntohs(header_->ip_len) - header_len();
        }

        void Cut(const uint32_t offset, const uint32_t data_len);

    private:
        const u_char* packet_;
        struct ip* header_;

        std::unique_ptr<TcpPacket> tcp_;
};

#endif  /* IP_PACKET_H_ */
