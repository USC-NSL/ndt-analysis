#include "tcp_sacks.h"

#include "util.h"

bool TcpSacks::Parse(const u_char* option, const size_t caplen) {
    sacks_.clear();
    num_sacks_ = 0;

    if (caplen < 2) {
        // Did not capture option size, so we cannot do anything here
        return true;
    }
    
    const uint8_t opt_size = *(option+1);
    if ((opt_size - 2) % 8 != 0 || opt_size > 40) {
        return false;
    }
    num_sacks_ = (opt_size - 2) / 8;
    size_t used_bytes = 2;

    // Read as many SACK blocks as we can a extract from the possibly
    // truncated header
    while (used_bytes + 8 <= caplen && used_bytes < opt_size) {
        Sack sack = {
            ntohl(*reinterpret_cast<const uint32_t*>(option+used_bytes)),
            ntohl(*reinterpret_cast<const uint32_t*>(option+used_bytes+4))
        };
        Add(sack);
        used_bytes += 8;
    }
    Merge();
    UpdateNumBytes();

    return true;
}

void TcpSacks::Add(Sack new_sack) {
    for (auto current_sack = sacks_.begin();
            current_sack != sacks_.end(); ++current_sack) {
        if (tcp_util::Before(new_sack.end_, current_sack->start_)) {
            // New item precedes current SACK
            sacks_.insert(current_sack, new_sack);
            UpdateNumBytes();
            return;
        }
        else if (tcp_util::Overlaps(
                    current_sack->start_, current_sack->end_,
                    new_sack.start_, new_sack.end_)) {
            // New item overlaps with current SACK, therefore extend the
            // existing range
            sacks_.insert(current_sack, new_sack);
            Merge();
            UpdateNumBytes();
            return;
        }
    }

    // New item does not precede any existing SACK, so it's added to the end of
    // the list
    sacks_.push_back(new_sack);
}

void TcpSacks::Add(TcpSacks new_sacks) {
    for (Sack new_sack : new_sacks.sacks_) {
        Add(new_sack);
    }
}

void TcpSacks::Merge() {
    if (sacks_.size() < 2) {
        return;
    }
    auto current_sack = sacks_.begin();
    auto next_sack = std::next(current_sack, 1);
    while (next_sack != sacks_.end()) {
        if (tcp_util::Overlaps(
                    current_sack->start_, current_sack->end_,
                    next_sack->start_, next_sack->end_)) {
            // Extend the range of the first SACK and remove the second
            // (overlapping) one
            if (tcp_util::Before(next_sack->start_, current_sack->start_)) {
                current_sack->start_ = next_sack->start_;
            }
            if (tcp_util::After(next_sack->end_, current_sack->end_)) {
                current_sack->end_ = next_sack->end_;
            }
            next_sack = sacks_.erase(next_sack);
        } else {
            current_sack = next_sack;
            ++next_sack;
        }
    }
}

void TcpSacks::RemoveAcked(uint32_t seq_acked) {
    auto current_sack = sacks_.begin();
    while (current_sack != sacks_.end()) {
        if (!tcp_util::After(current_sack->end_, seq_acked)) {
            current_sack = sacks_.erase(current_sack);
            continue;
        }
        if (tcp_util::Before(current_sack->start_, seq_acked)) {
            current_sack->start_ = seq_acked;
        }
        break;
    }
    UpdateNumBytes();
}

void TcpSacks::UpdateNumBytes() {
    num_bytes_ = 0;
    for (auto sack : sacks_) {
        num_bytes_ += (sack.end_ - sack.start_);
    }
}
