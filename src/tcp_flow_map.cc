#include "tcp_flow_map.h"

#include <iostream>

#include "ethernet_packet.h"
#include "ip_packet.h"
#include "packet.h"
#include "tcp_packet.h"

// Datalink type of the packets captured in the tcpdump. The type
// determines if and how the Ethernet header is parsed
int pcap_datalink_type_;

bool TcpFlowMap::AddPacket(std::unique_ptr<Packet> packet) {
    packet->set_index(index_++);

    TcpFlowId flow_id = {
        packet->ip()->src_addr(),
        packet->ip()->dst_addr(),
        packet->tcp()->src_port(),
        packet->tcp()->dst_port()
    };
    TcpFlowId rev_flow_id = {
        packet->ip()->dst_addr(),
        packet->ip()->src_addr(),
        packet->tcp()->dst_port(),
        packet->tcp()->src_port()
    };

    auto iter = map_.find(flow_id);
    if (iter == map_.end()) {
        iter = map_.find(rev_flow_id);
        if (iter == map_.end()) {
            map_[flow_id] = std::make_unique<TcpFlow>(flow_id);
        } else {
            flow_id = rev_flow_id;
        }
    }

    return map_[flow_id]->AddPacket(std::move(packet), true);
}

std::unique_ptr<TcpFlowMap> TcpFlowMapFactory::MakeFromPcap(
        const char* filename) {
    char errbuf[PCAP_ERRBUF_SIZE];

    pcap_t* pcap_handle = pcap_open_offline(filename, errbuf);
    if (pcap_handle == NULL) {
        std::cerr << "pcap_open_offline() failed: "
                  << errbuf << std::endl;
        return nullptr;
    }

    // Get the datalink type (determines if and how the Ethernet header is
    // extracted)
    pcap_datalink_type_ = pcap_datalink(pcap_handle);

    // Define function that processes each packet, i.e. adds it to the flow
    // map if it is a TCP packet
    auto process_packet_function =
        [](u_char* process_args, const struct pcap_pkthdr* pkthdr,
                const u_char* packet) {
        auto parsed_packet = std::make_unique<Packet>(packet, pkthdr);
        if (parsed_packet->is_tcp() &&
                !parsed_packet->tcp()->is_bogus()) {
            auto process_args_array = reinterpret_cast<void**>(process_args);
            auto pcap_handle = reinterpret_cast<pcap_t*>(process_args_array[0]);
            auto flow_map = reinterpret_cast<TcpFlowMap*>(process_args_array[1]);
            if (!flow_map->AddPacket(std::move(parsed_packet))) {
                pcap_breakloop(pcap_handle);
            }
        }
    };

    // Iterate through the PCAP and call the processing function for
    // each packet. Currently the function gets two arguments:
    // 1. the PCAP handle to break the loop if necessary
    // 2. the flow map to add the new packet to it
    auto map = std::make_unique<TcpFlowMap>();
    void* process_args[2] = { pcap_handle, map.get() };
    if (pcap_loop(pcap_handle, 0, process_packet_function,
                reinterpret_cast<u_char*>(process_args)) < 0) {
        std::cerr << "pcap_loop() failed: "
                  << pcap_geterr(pcap_handle) << std::endl;
        return nullptr;
    }
    pcap_close(pcap_handle);

    return map;
}
