#include "bms/core.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
std::vector<std::uint8_t> reply(const std::uint16_t tid=7){
    return {static_cast<std::uint8_t>(tid>>8),static_cast<std::uint8_t>(tid),0,0,0,15,1,3,12,0,235,3,232,1,194,0,1,0,1,1,244};
}
void should_throw_modbus(std::vector<std::uint8_t> r) {
    bool thrown=false;
    try {(void)bms::decode_modbus_reply(r,7,1,6);}catch(const std::exception&){thrown=true;}
    assert(thrown);
}
void tests(){
    bms::Ring<int> ring(2); ring.push(1);ring.push(2);ring.push(3);
    assert(ring.size()==2&&ring.dropped()==1&&ring.at(0)==2&&ring.at(1)==3);
    assert(ring.snapshot()==std::vector<int>({2,3}));
    bms::Co2Alarm alarm;
    assert(!alarm.observe(999));assert(alarm.observe(1000));assert(alarm.observe(950));
    assert(!alarm.observe(900));assert(!alarm.observe(901));
    bms::RetryBackoff backoff;
    assert(backoff.failure()==100);assert(backoff.failure()==200);
    for(int i=0;i<16;++i)backoff.failure();assert(backoff.failure()==3200);
    backoff.success();assert(backoff.failures()==0);
    const auto regs=bms::decode_modbus_reply(reply(),7,1,6);
    assert(regs.size()==6&&regs[0]==235&&regs[1]==1000&&regs[2]==450);
    auto p=bms::decode_registers(regs);
    assert(p.temp_deci_c==235&&p.co2_ppm==1000&&p.rh_deci_pct==450&&p.occupied&&p.hvac_on&&p.watts==500);
    bms::Sample s{0x1234,0x0102030405060708ULL,42,p,true};
    const auto buf=bms::encode_frame(s);
    assert(buf.size()==31&&buf[0]=='B'&&buf[3]=='1'&&buf[4]==0x12&&buf[5]==0x34);
    assert(buf[6]==1&&buf[13]==8&&buf[21]==42&&buf[28]==7&&buf[29]==1&&buf[30]==244);
    for (int kind=0; kind<3; ++kind) {
        auto invalid=s;
        if(kind==0)invalid.device_id=0;
        if(kind==1)invalid.sequence=0;
        if(kind==2)invalid.points.watts=50001;
        bool rejected=false;
        try{(void)bms::encode_frame(invalid);}catch(const std::invalid_argument&){rejected=true;}
        assert(rejected);
    }
    auto boundary=s;boundary.points.temp_deci_c=-500;boundary.points.watts=50000;
    assert(bms::encode_frame(boundary)[22]==0xfe);

    bms::Ring<std::uint64_t> latency(5);for(auto v:{10,30,20,50,40})latency.push(v);
    assert(bms::percentile_us(latency,0)==10&&bms::percentile_us(latency,50)==30&&bms::percentile_us(latency,99)==50);
    assert(bms::percentile_us(bms::Ring<std::uint64_t>(1),50)==0);
    auto bad=reply();bad[1]=8;should_throw_modbus(bad);
    bad=reply();bad[3]=1;should_throw_modbus(bad);
    bad=reply();bad[6]=2;should_throw_modbus(bad);
    bad=reply();bad[8]=10;should_throw_modbus(bad);
    bad=reply();bad[7]=0x83;should_throw_modbus(bad);
    bad=reply();bad.pop_back();should_throw_modbus(bad);
    bad=reply();bad[5]=254;should_throw_modbus(bad);
    bool thrown=false;
    try{(void)bms::decode_registers({1,2});}catch(const std::invalid_argument&){thrown=true;}
    assert(thrown);
    thrown=false;try{(void)bms::decode_registers({235,1000,1001,1,1,500});}catch(const std::invalid_argument&){thrown=true;}
    assert(thrown);
    thrown=false;try{(void)bms::decode_registers({235,1000,450,2,1,500});}catch(const std::invalid_argument&){thrown=true;}
    assert(thrown);
    std::cout<<"core tests passed\n";
}
}
int main(){tests();}
