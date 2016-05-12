#ifndef ETHERNET_PACKET_H_
#define ETHERNET_PACKET_H_

#include <memory>
#include <net/ethernet.h>
#include <netinet/ip.h>

#include "ip_packet.h"

// PCAP processing is done by the TCP Flow map which initially determines
// the datalink type of the packets captured in the tcpdump. The type
// determines if and how the Ethernet header is parsed
extern int pcap_datalink_type_;

class EthernetPacket {
    public:
        EthernetPacket(u_char* packet, const uint32_t caplen);

        inline const u_char* packet() const {
            return packet_;
        }
        inline IpPacket* ip() const {
            return ip_.get();
        }

        void Cut(const uint32_t offset, const uint32_t data_len);

    private:
        const u_char* packet_;
        struct ether_header* header_;

        std::unique_ptr<IpPacket> ip_;
};

#endif  /* ETHERNET_PACKET_H_ */
