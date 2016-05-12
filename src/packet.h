#ifndef PACKET_H_
#define PACKET_H_

#include <memory>
#include <pcap.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "ethernet_packet.h"

class Packet {
    public:
        Packet(const u_char* packet, const struct pcap_pkthdr* pcap_header);

        explicit Packet(const Packet& packet);

        EthernetPacket* ethernet() const;
        IpPacket* ip() const;
        TcpPacket* tcp() const;

        inline u_char* packet() const {
            return packet_.get();
        }
        inline bool is_tcp() const {
            return tcp() != nullptr;
        }
        inline uint64_t timestamp_us() const {
            return timestamp_us_;
        }
        inline uint32_t index() const {
            return index_;
        }
        inline Packet* next_packet() const {
            return next_packet_;
        }
        inline Packet* previous_packet() const {
            return previous_packet_;
        }
        inline Packet* previous_tx() const {
            return previous_tx_;
        }
        inline Packet* first_tx() const {
            return first_tx_;
        }
        inline Packet* rtx() const {
            return rtx_;
        }
        inline Packet* trigger_packet() const {
            return trigger_packet_;
        }
        inline bool out_of_order() const {
            return out_of_order_;
        }
        inline uint64_t bytes_passed() const {
            return bytes_passed_;
        }
        inline void set_index(const uint32_t index) {
            index_ = index;
        }
        inline void set_next_packet(Packet* packet) {
            next_packet_ = packet;
        }
        inline void set_previous_packet(Packet* packet) {
            previous_packet_ = packet;
        }
        inline void set_previous_tx(Packet* packet) {
            previous_tx_ = packet;
        }
        inline void set_first_tx(Packet* packet) {
            first_tx_ = packet;
        }
        inline void set_rtx(Packet* packet) {
            rtx_ = packet;
        }
        inline void set_trigger_packet(Packet* packet) {
            trigger_packet_ = packet;
        }
        inline void set_out_of_order(bool out_of_order) {
            out_of_order_ = out_of_order;
        }
        inline void set_bytes_passed(uint64_t bytes) {
            bytes_passed_ = bytes;
        }
        bool IsLost() const;

        bool IsFromSameEndpoint(const Packet& packet) const;

        Packet* CopyAndCut(const uint32_t offset,
                const uint32_t data_len) const;

    private:
        std::unique_ptr<u_char[]> packet_;
        std::unique_ptr<EthernetPacket> ethernet_;

        uint32_t caplen_;

        uint64_t timestamp_us_ = 0;
        uint32_t index_ = 0;

        Packet* next_packet_ = nullptr;
        Packet* previous_packet_ = nullptr;
        Packet* previous_tx_ = nullptr;
        Packet* first_tx_ = this;
        Packet* rtx_ = nullptr;
        Packet* trigger_packet_ = nullptr;

        bool out_of_order_ = false;

        uint64_t bytes_passed_ = 0;
};

#endif  /* PACKET_H_ */
