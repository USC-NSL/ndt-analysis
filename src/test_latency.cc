#include "gtest/gtest.h"

#include "delay_analysis.h"
#include "tcp_flow_map.h"

TEST(LatencyTest, Basic) {
    TcpFlowMapFactory flow_map_factory;
    auto flow_map = flow_map_factory.MakeFromPcap("tests/basic.pcap");

    ASSERT_NE(nullptr, flow_map);
    ASSERT_EQ(1, flow_map->map().size());

    const TcpFlow* flow = flow_map->map().begin()->second.get();
    ASSERT_NE(nullptr, flow);

    const TcpEndpoint* a = flow->endpoint_a();
    const TcpEndpoint* b = flow->endpoint_b();
    ASSERT_NE(nullptr, a);
    ASSERT_NE(nullptr, b);

    DelayAnalysis delay_a(*a);
    const Delays latency_a = delay_a.AnalyzeTailLatency();

    const uint64_t all_delays =
        latency_a.propagation_us_ + latency_a.loss_us_ +
        latency_a.loss_trigger_us_ + latency_a.queueing_us_ +
        latency_a.other_us_;
    EXPECT_LE(all_delays, latency_a.overall_us_);
    EXPECT_LE(latency_a.propagation_us_, latency_a.overall_us_);
    EXPECT_LE(latency_a.loss_us_, latency_a.overall_us_);
    EXPECT_LE(latency_a.loss_trigger_us_, latency_a.overall_us_);
    EXPECT_LE(latency_a.queueing_us_, latency_a.overall_us_);
    EXPECT_LE(latency_a.other_us_, latency_a.overall_us_);

    // Endpoint B is the receiver and does not produce any data
    DelayAnalysis delay_b(*b);
    const Delays latency_b = delay_b.AnalyzeTailLatency();
    EXPECT_EQ(0, b->GetNumDataPackets());
    EXPECT_EQ(0, latency_b.overall_us_);
    EXPECT_EQ(0, latency_b.propagation_us_);
    EXPECT_EQ(0, latency_b.loss_us_);
    EXPECT_EQ(0, latency_b.loss_trigger_us_);
    EXPECT_EQ(0, latency_b.queueing_us_);
    EXPECT_EQ(0, latency_b.other_us_);
}

TEST(LatencyTest, TLPAndRTO) {
    TcpFlowMapFactory flow_map_factory;
    auto flow_map = flow_map_factory.MakeFromPcap("tests/tlp-and-rto.pcap");

    ASSERT_NE(nullptr, flow_map);
    ASSERT_EQ(1, flow_map->map().size());

    const TcpFlow* flow = flow_map->map().begin()->second.get();
    ASSERT_NE(nullptr, flow);

    const TcpEndpoint* a = flow->endpoint_a();
    const TcpEndpoint* b = flow->endpoint_b();
    ASSERT_NE(nullptr, a);
    ASSERT_NE(nullptr, b);

    DelayAnalysis delay_b(*b);
    const Delays latency_b = delay_b.AnalyzeTailLatency();

    // The worst packet is delayed by:
    // 1. Queueing delay of the last packet arming the timer (150ms)
    // 2. TLP timeout (390ms, 90ms caused by queueing)
    // 3. 2x RTO (430ms + 865ms, 110ms + 230ms caused by queueing)
    // 4. Base propagation delay (2x100ms, for armer and last transmission)
    EXPECT_NEAR(latency_b.overall_us_, 2035E3, 10E3);
    EXPECT_NEAR(latency_b.propagation_us_, 100E3, 10E3);
    EXPECT_NEAR(latency_b.loss_us_, 1400E3, 10E3);
    EXPECT_NEAR(latency_b.loss_trigger_us_, 535E3, 10E3);
    EXPECT_NEAR(latency_b.queueing_us_, 0E3, 10E3);
    EXPECT_NEAR(latency_b.other_us_, 0, 10E3);
}

TEST(LatencyTest, RTOOnly) {
    TcpFlowMapFactory flow_map_factory;
    auto flow_map = flow_map_factory.MakeFromPcap("tests/rto-only.pcap");

    ASSERT_NE(nullptr, flow_map);
    ASSERT_EQ(1, flow_map->map().size());

    const TcpFlow* flow = flow_map->map().begin()->second.get();
    ASSERT_NE(nullptr, flow);

    const TcpEndpoint* a = flow->endpoint_a();
    const TcpEndpoint* b = flow->endpoint_b();
    ASSERT_NE(nullptr, a);
    ASSERT_NE(nullptr, b);

    DelayAnalysis delay_b(*b);
    const Delays latency_b = delay_b.AnalyzeTailLatency();

    // The worst packet is delayed by:
    // 1. Queueing delay of the last packet arming the timer (100ms)
    // 2. First RTO (430ms, 120ms caused by queueing)
    // 3. Second RTO (865ms, 235ms caused by queueing)
    // 4. Base propagation delay (2x100ms, for armer and last transmission)
    EXPECT_NEAR(latency_b.overall_us_, 1645E3, 10E3);
    EXPECT_NEAR(latency_b.propagation_us_, 100E3, 10E3);
    EXPECT_NEAR(latency_b.loss_us_, 1085E3, 10E3);
    EXPECT_NEAR(latency_b.loss_trigger_us_, 455E3, 10E3);
    EXPECT_NEAR(latency_b.queueing_us_, 0E3, 10E3);
    EXPECT_NEAR(latency_b.other_us_, 0, 10E3);
}

TEST(LatencyTest, RTOAndSlowStart) {
    TcpFlowMapFactory flow_map_factory;
    auto flow_map = flow_map_factory.MakeFromPcap("tests/rto-and-slow-start.pcap");

    ASSERT_NE(nullptr, flow_map);
    ASSERT_EQ(1, flow_map->map().size());

    const TcpFlow* flow = flow_map->map().begin()->second.get();
    ASSERT_NE(nullptr, flow);

    const TcpEndpoint* a = flow->endpoint_a();
    const TcpEndpoint* b = flow->endpoint_b();
    ASSERT_NE(nullptr, a);
    ASSERT_NE(nullptr, b);

    DelayAnalysis delay_b(*b);
    const Delays latency_b = delay_b.AnalyzeTailLatency();

    // The worst packet is delayed by:
    // 1. Queueing delay of the last packet arming the timer (20ms)
    // 2. RTO (300ms)
    // 3. Non-queueing delay of the RTO retransmission (100ms)
    // 4. Base propagation delay (200ms, for armer and last transmission)
    EXPECT_NEAR(latency_b.overall_us_, 620E3, 10E3);
    EXPECT_NEAR(latency_b.propagation_us_, 100E3, 10E3);
    EXPECT_NEAR(latency_b.loss_us_, 500E3, 10E3);
    EXPECT_NEAR(latency_b.loss_trigger_us_, 20E3, 10E3);
    EXPECT_NEAR(latency_b.queueing_us_, 0E3, 10E3);
    EXPECT_NEAR(latency_b.other_us_, 0, 10E3);
}

TEST(LatencyTest, QueuingOnly) {
    TcpFlowMapFactory flow_map_factory;
    auto flow_map = flow_map_factory.MakeFromPcap("tests/queuing-only.pcap");

    ASSERT_NE(nullptr, flow_map);
    ASSERT_EQ(1, flow_map->map().size());

    const TcpFlow* flow = flow_map->map().begin()->second.get();
    ASSERT_NE(nullptr, flow);

    const TcpEndpoint* a = flow->endpoint_a();
    const TcpEndpoint* b = flow->endpoint_b();
    ASSERT_NE(nullptr, a);
    ASSERT_NE(nullptr, b);

    DelayAnalysis delay_b(*b);
    const Delays latency_b = delay_b.AnalyzeTailLatency();

    // The worst packet is delayed by:
    // 1. Queueing delay of the last packet (100ms)
    // 2. Base propagation delay (100ms)
    // 3. Other (50ms)
    EXPECT_NEAR(latency_b.overall_us_, 250E3, 10E3);
    EXPECT_NEAR(latency_b.propagation_us_, 100E3, 10E3);
    EXPECT_NEAR(latency_b.loss_us_, 0E3, 0E3);
    EXPECT_NEAR(latency_b.loss_trigger_us_, 0E3, 0E3);
    EXPECT_NEAR(latency_b.queueing_us_, 100E3, 10E3);
    EXPECT_NEAR(latency_b.other_us_, 50E3, 10E3);
}
