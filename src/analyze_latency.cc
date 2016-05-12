#include <fstream>
#include <glog/logging.h>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "delay_analysis.h"
#include "packet.h"
#include "tcp_endpoint.h"
#include "tcp_flow_map.h"
#include "tcp_packet.h"
#include "util.h"

const std::vector<std::string> kDirections = { "a2b", "b2a" };
const std::vector<uint16_t> kPercentiles = {10, 25, 50, 75, 90};
const std::vector<uint32_t> kTimerRelativeSeqs = {
    1, 20*1024, 50*1024, 100*1024, 200*1024, 500*1024, 1000*1024};
const uint32_t kEarlyTailPerformerMaxSeq = 102400;

void PrintOutputFormat() {
    std::vector<std::string> fields;
    fields.push_back("Input filename");
    fields.push_back("Flow index");
    fields.push_back("Direction");
    fields.push_back("# data packets");
    fields.push_back("# lost packets");
    fields.push_back("# missing trigger packets");
    for (std::string prefix : {"All: "}) {
        fields.push_back(prefix + "Tail latency (overall, in microseconds)");
        fields.push_back(prefix + "Tail latency (from propagation)");
        fields.push_back(prefix + "Tail latency (from loss)");
        fields.push_back(prefix + "Tail latency (from loss trigger)");
        fields.push_back(prefix + "Tail latency (from queueing)");
        fields.push_back(prefix + "Tail latency (from other)");
        fields.push_back(prefix + "Tail latency (from no-queue timeout)");
        fields.push_back(prefix + "Trigger breakdown: from timeout");
        fields.push_back(prefix + "Trigger breakdown: from late ACK arming");
        fields.push_back(prefix + "Trigger breakdown: from late ACK triggering");
        fields.push_back(prefix + "Trigger breakdown: from late trigger(s) for final trigger");
        fields.push_back(prefix + "Unacked bytes/RTT Pearson correlation coefficient");
        fields.push_back(prefix + "c_0 value of linear fit (y = c_0 + c_1 * x)");
        fields.push_back(prefix + "c_1 value of linear fit");
        fields.push_back(prefix + "Sum-squared error of linear fit");
        fields.push_back(prefix + "Goodput before worst packet (bps)");
        fields.push_back(prefix + "Bytes acked before worst packet");
        fields.push_back(prefix + "Bytes needed buffered (to compensate worst packet delay)");
        fields.push_back(prefix + "Bytes unacked before worst packet");
    }
    for (auto seq : kTimerRelativeSeqs) {
        auto seq_str = std::to_string(seq);
        fields.push_back("Seq " + seq_str + ": RTO estimate");
        fields.push_back("Seq " + seq_str + ": TLP estimate");
        fields.push_back("Seq " + seq_str + ": TLP+delayed ACK estimate");
        fields.push_back("Seq " + seq_str + ": Queue-free RTO estimate");
        fields.push_back("Seq " + seq_str + ": Queue-free TLP estimate");
        fields.push_back("Seq " + seq_str + ": Queue-free TLP+delayed ACK estimate");
    }

    // TODO Generates lots of output, so we omit this for now
    // fields.push_back("# Unacked bytes/RTT pairs");
    // fields.push_back("[Multiple columns] Raw pairs");

    uint16_t column_index = 1;
    for (const std::string field : fields) {
        std::cout << std::setw(2) << column_index++ << " "
                  << field << std::endl;
    }
}

int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);

    if (argc != 2) {
        std::cerr << "Wrong number of parameters." << std::endl
                  << "Usage: " << argv[0]
                  << " -p|<pcap filename>" << std::endl;
        return 1;
    }
    const std::string print_option("-p");
    if (print_option.compare(argv[1]) == 0) {
        PrintOutputFormat();
        return 0;
    }
   
    TcpFlowMapFactory flow_map_factory;
    auto flow_map = flow_map_factory.MakeFromPcap(argv[1]);
    if (flow_map == nullptr) {
        return 1;
    }
    
    const std::string input_filename = std::string(argv[1]);
    uint16_t flow_index = 0;
    for (auto const& mapped_flow : flow_map->map()) {
        const TcpFlow& flow = *(mapped_flow.second.get());
        for (auto direction : kDirections) {
            const TcpEndpoint* sender = (direction == "a2b") ?
                flow.endpoint_a() : flow.endpoint_b();
            const TcpEndpoint* receiver = (direction == "a2b") ?
                flow.endpoint_b() : flow.endpoint_a();
            if (sender == nullptr || receiver == nullptr ||
                    sender->is_bogus()) {
                VLOG(1) << "Endpoint has bogus data. Skipping.";
                continue;
            }
            
            // Output metadata
            std::cout << input_filename << ","
                      << flow_index << ","
                      << direction << ","
                      << sender->GetNumDataPackets() << ","
                      << sender->GetNumLosses() << ","
                      << sender->GetNumMissingTriggerPackets() << ",";

            // Output analysis:
            // a. for the tail performer among all packets
            // b. for the tail performer carrying a seqno <
            // kEarlyTailPerformerMaxSeq
            DelayAnalysis delay_analysis(*sender);
            // for (auto max_seq :
            //        std::initializer_list<uint32_t>{0, kEarlyTailPerformerMaxSeq}) {
            for (auto max_seq : std::initializer_list<uint32_t>{0}) {
                // Output tail latency summary
                Delays tail_latency = delay_analysis.AnalyzeTailLatency(max_seq);
                std::cout << tail_latency.overall_us_ << ","
                          << tail_latency.propagation_us_ << ","
                          << tail_latency.loss_us_ << ","
                          << tail_latency.loss_trigger_us_ << ","
                          << tail_latency.queueing_us_ << ","
                          << tail_latency.other_us_ << ",";

                // Output trigger breakdown
                TriggerDelays trigger_breakdown = tail_latency.loss_trigger_breakdown_;
                std::cout << trigger_breakdown.no_queue_timeout_us_ << ","
                          << trigger_breakdown.timeout_us_ << ","
                          << trigger_breakdown.late_ack_arms_us_ << ","
                          << trigger_breakdown.late_ack_triggers_us_ << ","
                          << trigger_breakdown.late_trigger_for_trigger_us_ << ",";

                // Output correlation and best linear fit parameters
                auto correlation = delay_analysis.correlation();
                auto fit = delay_analysis.fit();
                std::cout << correlation << ","
                          << fit.c_0 << ","
                          << fit.c_1 << ","
                          << fit.sum_sq << ",";

                // Goodput metrics
                std::cout << tail_latency.goodput_before_worst_packet_bps_ << ","
                          << tail_latency.bytes_acked_before_worst_packet_ << ","
                          << tail_latency.bytes_needed_buffered_ << ","
                          << tail_latency.bytes_unacked_ << ",";
            }

            // Timer estimates (make sure this is preceded by the right analysis
            // to tag the worst packet and compute the proper queuing delays)
            auto estimate_list = delay_analysis.GetTimerEstimates(kTimerRelativeSeqs);
            for (auto estimates : estimate_list) {
                std::cout << estimates.rto_us_ << ","
                          << estimates.tlp_us_ << ","
                          << estimates.tlp_delayed_ack_us_ << ","
                          << estimates.queue_free_rto_us_ << ","
                          << estimates.queue_free_tlp_us_ << ","
                          << estimates.queue_free_tlp_delayed_ack_us_ << ",";
            }

            // TODO Generates lots of output, so we omit this for now
            // auto bytes_rtt_pairs = sender->GetUnackedBytesRttPairs();
            // std::vector<double> rtts, unacked_bytes;
            // vector_util::SplitPairs(bytes_rtt_pairs, &unacked_bytes, &rtts);
            // std::cout << bytes_rtt_pairs.size();
            // if (bytes_rtt_pairs.empty()) {
            //     std::cout << std::endl;
            //     continue;
            // }

            // Print binned unacked bytes/RTT pairs
            // auto populated_bins = stats_util::PopulatedHistogramBins(
            //         bytes_rtt_pairs, 1024, 1000);
            // for (auto bin : populated_bins) {
            //     std::cout << "," << (int) bin.first
            //               << "," << (int) bin.second;
            // }
            std::cout << std::endl;
        }
        flow_index++;
    }

    return 0;
}
