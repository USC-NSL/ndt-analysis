#ifndef TCP_FLOW_MAP_H_
#define TCP_FLOW_MAP_H_

#include <map>
#include <memory>
#include <netinet/ip.h>
#include <pcap.h>

#include "packet.h"
#include "tcp_flow.h"

class TcpFlowMap {
    public:
        // Adds a new packet to the matching flow in this flow map. If no
        // matching flow exists yet, a new one is created. Mapping is
        // based on TcpFlowId and does NOT handle potentially separate
        // flows with the same TcpFlowId (e.g. a reused TCP connection)
        // Returns TRUE, unless there is an indication that the sending endpoint
        // deals with bogus data
        bool AddPacket(std::unique_ptr<Packet> packet);

        const std::map<TcpFlowId, std::unique_ptr<TcpFlow>>& map() const {
            return map_;
        }

    private:
        std::map<TcpFlowId, std::unique_ptr<TcpFlow>> map_;

        // Running index for packets added to the map
        uint32_t index_ = 0;
};

class TcpFlowMapFactory {
    public:
        // Creates and populates a new TcpFlowMap based on packets parsed from a
        // PCAP file
        std::unique_ptr<TcpFlowMap> MakeFromPcap(const char* filename);

    private:
        pcap_t* pcap_handle_ = nullptr;
};

#endif  /* TCP_FLOW_MAP_H_ */
