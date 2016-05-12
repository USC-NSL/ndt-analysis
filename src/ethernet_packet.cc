#include "ethernet_packet.h"

#include <arpa/inet.h>

// Linux-cooked header definition (as described in libpcap's sll.h)
typedef struct pcap_sll_header {
    static const uint16_t LINUX_SLL_HOST      = 0;
    static const uint16_t LINUX_SLL_BROADCAST = 1;
    static const uint16_t LINUX_SLL_MULTICAST = 2;
    static const uint16_t LINUX_SLL_OTHERHOST = 3;
    static const uint16_t LINUX_SLL_OUTGOING  = 4;

    uint16_t  pkttype;    // packet type
    uint16_t  hatype;     // link-layer address type
    uint16_t  halen;      // link-layer address length
    uint8_t   addr[8];    // link-layer address
    uint16_t  protocol;   // protocol
} pcap_sll_header;

EthernetPacket::EthernetPacket(u_char* packet, const uint32_t caplen)
        : packet_(packet),
          header_(nullptr),
          ip_(nullptr) {
    // We only allow Ethernet and Linux-cooked headers right now
    size_t header_len = 0;
    uint16_t protocol = 0;
    pcap_sll_header* sll;
    switch(pcap_datalink_type_) {
        case DLT_EN10MB:  /* Ethernet */
            header_len = sizeof(struct ether_header);
            if (caplen < header_len) {
                return;
            }
            header_ = (struct ether_header*) packet;
            protocol = ntohs(header_->ether_type);
            break;
        case DLT_LINUX_SLL:  /* Linux-cooked header */
            header_len = sizeof(struct pcap_sll_header);
            if (caplen < header_len) {
                return;
            }
            sll = (pcap_sll_header*) packet;
            protocol = ntohs(sll->protocol);
            break;
        default:  /* Unsupported datalink type */
            return;
    }

    if (protocol == ETHERTYPE_IP) {
        u_char* ip_packet = packet + header_len;
        const int32_t ip_caplen = caplen - header_len;
        
        if (ip_caplen > 0) {
            ip_ = std::make_unique<IpPacket>(ip_packet, ip_caplen);
        }
    }
}

void EthernetPacket::Cut(const uint32_t offset, const uint32_t data_len) {
    ip_->Cut(offset, data_len);
}
