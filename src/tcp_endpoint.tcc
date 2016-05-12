template<typename ReturnType>
uint32_t TcpEndpoint::GetCountForNonZeroFunctionValue(
        PacketFunction<ReturnType> func) const {
    uint32_t count = 0;
    for (const Packet* packet : packets_) {
        if ((packet->*func)()) {
            count++;
        }
    }
    return count;
}

template<typename ReturnType>
uint32_t TcpEndpoint::GetCountForNonZeroFunctionValue(
        TcpPacketFunction<ReturnType> func) const {
    uint32_t count = 0;
    for (const Packet* packet : packets_) {
        if ((packet->tcp()->*func)()) {
            count++;
        }
    }
    return count;
}

template<typename ReturnType>
std::vector<ReturnType> TcpEndpoint::CollectFunctionValues(
        TcpPacketFunction<ReturnType> func) const {
    std::vector<ReturnType> values;
    for (const Packet* packet : packets_) {
        if ((packet->tcp()->*func)()) {
            values.push_back((packet->tcp()->*func)());
        }
    }
    return values;
}
