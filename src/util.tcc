namespace stats_util {

template<typename Number>
Number Percentile(const std::vector<Number> values, const uint8_t percentile,
        const bool input_is_sorted) {
    std::vector<Number> copy = values;
    if (copy.empty()) {
        return 0;
    }
    if (!input_is_sorted) {
        std::sort(copy.begin(), copy.end());
    }
    
    uint32_t fetch_position = copy.size() * percentile / 100;
    if (fetch_position == copy.size()) {
        fetch_position--;
    }
    return copy[fetch_position];
}

template<typename Number>
inline Number Mean(const std::vector<Number> values) {
    if (values.empty()) {
        return 0;
    }
    Number sum = 0;
    for (const Number value : values) {
        sum += value;
    }
    return sum / ((Number) values.size());
}

}  // namespace stats_util

namespace vector_util {

template<typename Number>
void SplitPairs(const std::vector<std::pair<Number, Number>> pairs,
        std::vector<Number>* firsts,
        std::vector<Number>* seconds) {
    for (auto pair : pairs) {
        firsts->push_back(pair.first);
        seconds->push_back(pair.second);
    }
}

}  // namespace vector_util
