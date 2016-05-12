#ifndef TCP_FLOW_H_
#define TCP_FLOW_H_

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "packet.h"
#include "tcp_endpoint.h"

typedef struct TcpFlowId {
    uint32_t src_addr;
    uint32_t dst_addr;
    uint16_t src_port;
    uint16_t dst_port;

    bool operator==(const TcpFlowId& id) const {
        return (src_addr == id.src_addr &&
                dst_addr == id.dst_addr &&
                src_port == id.src_port &&
                dst_port == id.dst_port);
    }

    bool operator<(const TcpFlowId& id) const {
        if (src_addr == id.src_addr) {
            if (dst_addr == id.dst_addr) {
                if (src_port == id.src_port) {
                    return dst_port < id.dst_port;
                }
                return src_port < id.src_port;
            }
            return dst_addr < id.dst_addr;
        }
        return src_addr < id.src_addr;
    }

    std::string str() const {
        std::ostringstream buffer;
        buffer << src_addr << ":" << src_port << " -> "
               << dst_addr << ":" << dst_port;
        return buffer.str();
    }
} TcpFlowId;

class TcpFlow {
    public:
        explicit TcpFlow(const TcpFlowId& id);

        // Returns TRUE, unless there is an indication that the sending endpoint
        // saw bogus data
        bool AddPacket(std::unique_ptr<Packet> packet,
                bool process_packet);
        
        bool AddPacket(Packet* packet, bool process_packet);

        bool AddPacketToEndpoint(Packet* packet, bool process_packet);

        inline const TcpFlowId id() const {
            return id_;
        }
        inline TcpEndpoint* endpoint_a() const {
            return endpoint_a_.get();
        }
        inline TcpEndpoint* endpoint_b() const {
            return endpoint_b_.get();
        }

        std::vector<std::unique_ptr<TcpFlow>> SplitIntoSegments() const;

    private:
        // Extract and buffer the maximum segment size (MSS) value if possible
        void CheckForMSS(const Packet& packet);

        std::vector<Packet*> packets_;
        std::vector<std::unique_ptr<Packet>> owned_packets_;

        const TcpFlowId id_;
        std::unique_ptr<TcpEndpoint> endpoint_a_;
        std::unique_ptr<TcpEndpoint> endpoint_b_;

        // Maximum segment size (MSS) advertised during the initial handshake.
        // The MSS that endpoint A can used is advertised in the SYN coming
        // from endpoint B and vice versa
        uint32_t mss_a_ = 0;
        uint32_t mss_b_ = 0;
};

#endif  /* TCP_FLOW_H_ */
