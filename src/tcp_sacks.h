#ifndef TCP_SACKS_H_
#define TCP_SACKS_H_

#include <list>
#include <netinet/in.h>
#include <pcap.h>
#include "stdint.h"

typedef struct Sack {
    uint32_t start_;
    uint32_t end_;
} Sack;

class TcpSacks {
    public:
        inline bool empty() const {
            return num_sacks_ == 0;
        }
        inline uint32_t num_sacks() const {
            return num_sacks_;
        }
        inline uint32_t num_bytes() const {
            return num_bytes_;
        }
        inline std::list<Sack> sacks() const {
            return sacks_;
        }

        // Parses the SACK option. Returns TRUE if parsing was successful
        bool Parse(const u_char* option, const size_t caplen);

        // Adds a new SACK block to the given list and potentially merges
        // existing ranges
        void Add(Sack new_sack);

        // Adds new SACK blocks to the given list and potentially merges
        // existing ranges
        void Add(TcpSacks new_sacks);

        // Merge overlapping SACK blocks
        void Merge();

        // Move or cut all ranges that precede the given ACK number
        void RemoveAcked(uint32_t seq_acked);
    
    private:
        // Recompute the number of bytes covered by the SACK blocks
        void UpdateNumBytes();

        std::list<Sack> sacks_;
        
        // The options block of a packet header might be truncated and
        // we potentially did not see the actual SACK blocks and can only
        // compute the number of SACK blocks passed on the length of the
        // header option. As such the SACKs vector above might carry
        // fewer elements than counted here.
        // This is only set in the parsing phase and NOT updated when new SACK
        // blocks are added later or merged
        uint32_t num_sacks_ = 0;

        // Number of bytes covered by the stored SACK blocks
        uint32_t num_bytes_ = 0;
};

#endif  /* TCP_SACKS_H_ */

