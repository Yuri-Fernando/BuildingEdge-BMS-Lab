#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace bms {
struct Points {
    std::int16_t temp_deci_c{};
    std::uint16_t co2_ppm{};
    std::uint16_t rh_deci_pct{};
    bool occupied{};
    bool hvac_on{};
    std::uint16_t watts{};
};
struct Sample {
    std::uint16_t device_id{1};
    std::uint64_t sequence{};
    std::uint64_t timestamp_ms{};
    Points points{};
    bool alarm{};
};

class Co2Alarm {
    bool active_ = false;
public:
    bool observe(std::uint16_t ppm) noexcept {
        if (!active_ && ppm >= 1000) active_ = true;
        else if (active_ && ppm <= 900) active_ = false;
        return active_;
    }
    bool active() const noexcept { return active_; }
};

template<typename T> class Ring {
    std::vector<T> items_;
    std::size_t begin_ = 0, count_ = 0, dropped_ = 0;
public:
    explicit Ring(std::size_t capacity) : items_(capacity) {
        if (capacity == 0 || capacity > 65536) throw std::invalid_argument("ring capacity out of range");
    }
    void push(const T& value) {
        if (count_ < items_.size()) {
            items_[(begin_ + count_) % items_.size()] = value;
            ++count_;
        } else {
            items_[begin_] = value;
            begin_ = (begin_ + 1) % items_.size();
            ++dropped_;
        }
    }
    std::size_t size() const noexcept { return count_; }
    std::size_t capacity() const noexcept { return items_.size(); }
    std::size_t dropped() const noexcept { return dropped_; }
    const T& at(std::size_t i) const {
        if (i >= count_) throw std::out_of_range("ring index");
        return items_[(begin_ + i) % items_.size()];
    }
    std::vector<T> snapshot() const {
        std::vector<T> result;
        result.reserve(count_);
        for(std::size_t i=0;i<count_;++i) result.push_back(at(i));
        return result;
    }
};

class RetryBackoff {
    unsigned failures_ = 0;
public:
    void success() noexcept { failures_ = 0; }
    unsigned failure() noexcept {
        if (failures_ < 16) ++failures_;
        const unsigned shift = failures_ > 6 ? 6 : failures_;
        const unsigned ms = 50u << shift;
        return ms > 5000u ? 5000u : ms;
    }
    unsigned failures() const noexcept { return failures_; }
};

constexpr std::size_t kFrameSize = 31;
std::array<std::uint8_t, kFrameSize> encode_frame(const Sample& sample);
Points decode_registers(const std::vector<std::uint16_t>& regs);
std::vector<std::uint16_t> decode_modbus_reply(const std::vector<std::uint8_t>& response,
                                                std::uint16_t expected_tid,
                                                std::uint8_t expected_unit,
                                                std::uint16_t count);
std::uint64_t percentile_us(const Ring<std::uint64_t>& values, unsigned percent);
}
