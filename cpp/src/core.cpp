#include "bms/core.hpp"
#include <algorithm>

namespace bms {
namespace {
void put16(std::array<std::uint8_t, kFrameSize>& out, std::size_t pos, std::uint16_t x) {
    out[pos]=static_cast<std::uint8_t>(x>>8);
    out[pos+1]=static_cast<std::uint8_t>(x);
}
void put64(std::array<std::uint8_t, kFrameSize>& out, std::size_t pos, std::uint64_t x) {
    for (int i=7;i>=0;--i) {out[pos+static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(x);x >>= 8;}
}
std::uint16_t get16(const std::vector<std::uint8_t>& in, std::size_t pos) {
    return static_cast<std::uint16_t>((in[pos] << 8) | in[pos+1]);
}
}
std::array<std::uint8_t, kFrameSize> encode_frame(const Sample& s) {
    if (s.device_id == 0 || s.sequence == 0 || s.points.watts > 50000 ||
        s.points.rh_deci_pct > 1000 || s.points.co2_ppm > 10000 ||
        s.points.temp_deci_c < -500 || s.points.temp_deci_c > 1250)
        throw std::invalid_argument("telemetry values out of range");
    std::array<std::uint8_t, kFrameSize> b{};
    b[0]='B'; b[1]='M'; b[2]='S'; b[3]='1';
    put16(b,4,s.device_id);put64(b,6,s.sequence);put64(b,14,s.timestamp_ms);
    put16(b,22,static_cast<std::uint16_t>(s.points.temp_deci_c));
    put16(b,24,s.points.co2_ppm); put16(b,26,s.points.rh_deci_pct);
    b[28]=(s.points.occupied?1:0)|(s.points.hvac_on?2:0)|(s.alarm?4:0);
    put16(b,29,s.points.watts);
    return b;
}
Points decode_registers(const std::vector<std::uint16_t>& regs) {
    if (regs.size() != 6) throw std::invalid_argument("expected exactly six registers");
    Points p{};
    p.temp_deci_c=static_cast<std::int16_t>(regs[0]);p.co2_ppm=regs[1];p.rh_deci_pct=regs[2];
    if (regs[3]>1 || regs[4]>1) throw std::invalid_argument("invalid boolean register");
    p.occupied=regs[3]!=0;p.hvac_on=regs[4]!=0;p.watts=regs[5];
    if(p.temp_deci_c < -500 || p.temp_deci_c > 1250 ||p.co2_ppm > 10000 ||p.rh_deci_pct > 1000 ||p.watts>50000)
        throw std::invalid_argument("sensor values outside configured limits");
    return p;
}
std::vector<std::uint16_t> decode_modbus_reply(const std::vector<std::uint8_t>& r,
                                                std::uint16_t tid, std::uint8_t unit,
                                                std::uint16_t count) {
    if(count==0 || count>125 || r.size()<9) throw std::runtime_error("invalid Modbus response length");
    const auto length=get16(r,4);
    if (get16(r,0)!=tid || get16(r,2)!=0 || r[6]!=unit || length != r.size()-6)
        throw std::runtime_error("Modbus transaction, unit or MBAP mismatch");
    if(r[7]==0x83) throw std::runtime_error("Modbus exception response");
    if(r[7]!=0x03 || r[8] != count*2 || r.size()!=9u+2u*count)
        throw std::runtime_error("Modbus function or byte count mismatch");
    std::vector<std::uint16_t> regs;
    regs.reserve(count);
    for(std::size_t i=0;i<count;++i) regs.push_back(get16(r,9+2*i));
    return regs;
}
std::uint64_t percentile_us(const Ring<std::uint64_t>& vals, unsigned pct) {
    if (pct>100) throw std::invalid_argument("percentile > 100");
    auto v=vals.snapshot();if(v.empty())return 0;
    std::sort(v.begin(),v.end());
    const auto pos = static_cast<std::size_t>((static_cast<std::uint64_t>(v.size()-1)*pct+99)/100);
    return v[pos];
}
}
