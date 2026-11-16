#include "bms/core.hpp"
#include "bms/net.hpp"
#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <atomic>

namespace {
std::atomic<bool> stop_requested{false};
void on_signal(int){stop_requested.store(true);}
std::uint64_t now_ms(){using namespace std::chrono;return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();}
struct Options {
    std::string host="127.0.0.1", collector="127.0.0.1",metrics_file="";
    int port=15020,collector_port=17001,poll_ms=100,rounds=60,ring_capacity=256,unit=1,device_id=1;
    bool no_collector=false;
};
int number(const std::string& raw,int low,int high,const std::string& field){
    std::size_t pos=0;int value=0;
    try{value=std::stoi(raw,&pos);}catch(...){throw std::invalid_argument(field+" is not an integer");}
    if(pos!=raw.size()||value<low||value>high)throw std::invalid_argument(field+" out of range");
    return value;
}
Options parse(int argc,char** argv){
    Options o;
    for(int i=1;i<argc;++i){
        auto key=std::string(argv[i]);
        if(key=="--no-collector"){o.no_collector=true;continue;}
        if(key=="--help") {std::cout<<"bms-edge --host HOST --port PORT --collector HOST --collector-port PORT --poll-ms N --rounds N --unit N --device-id N --ring-capacity N --metrics-file PATH [--no-collector]\n";std::exit(0);}
        if(i+1>=argc)throw std::invalid_argument("missing value for "+key);
        auto val=std::string(argv[++i]);
        if(key=="--host")o.host=val;
        else if(key=="--collector")o.collector=val;
        else if(key=="--port")o.port=number(val,1,65535,key);
        else if(key=="--collector-port")o.collector_port=number(val,1,65535,key);
        else if(key=="--poll-ms")o.poll_ms=number(val,1,60000,key);
        else if(key=="--rounds")o.rounds=number(val,0,1000000,key);
        else if(key=="--ring-capacity")o.ring_capacity=number(val,1,65536,key);
        else if(key=="--unit")o.unit=number(val,1,247,key);
        else if(key=="--device-id")o.device_id=number(val,1,65535,key);
        else if(key=="--metrics-file")o.metrics_file=val;
        else throw std::invalid_argument("unknown option: "+key);
    }
    return o;
}
}
int main(int argc,char** argv){
    try {
        std::signal(SIGINT,on_signal);std::signal(SIGTERM,on_signal);
        const Options o=parse(argc,argv);
        bms::ModbusClient modbus(o.host,static_cast<std::uint16_t>(o.port));
        bms::CollectorClient collector(o.collector,static_cast<std::uint16_t>(o.collector_port));
        bms::Co2Alarm alarm;
        bms::RetryBackoff backoff;
        bms::Ring<std::uint64_t> latencies(2048);
        bms::Ring<bms::Sample> samples(static_cast<std::size_t>(o.ring_capacity));
        unsigned successes=0,modbus_errors=0,validation_errors=0,collector_errors=0,alarm_edges=0;
        std::uint64_t seq=0;
        bool old_alarm=false;
        for(std::uint64_t attempt=0;(o.rounds==0||attempt<static_cast<std::uint64_t>(o.rounds))&&!stop_requested.load();++attempt){
            const auto start=std::chrono::steady_clock::now();
            unsigned delay=static_cast<unsigned>(o.poll_ms);
            try {
                auto regs=modbus.read_holding(static_cast<std::uint8_t>(o.unit),0,6);
                const auto point=bms::decode_registers(regs);
                const auto poll_us=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count());
                latencies.push(poll_us);
                bool current_alarm=alarm.observe(point.co2_ppm);
                if(current_alarm!=old_alarm){++alarm_edges;old_alarm=current_alarm;}
                const bms::Sample s{static_cast<std::uint16_t>(o.device_id),++seq,now_ms(),point,current_alarm};
                samples.push(s);
                ++successes;backoff.success();
                if(!o.no_collector){
                    try{collector.send(bms::encode_frame(s));}catch(const std::exception& e){
                        ++collector_errors;std::cerr<<"collector error: "<<e.what()<<'\n';
                    }
                }
                std::cout<<"{\"device\":"<<s.device_id<<",\"seq\":"<<s.sequence
                         <<",\"ts_ms\":"<<s.timestamp_ms<<",\"temp_c\":"<<std::fixed<<std::setprecision(1)<<point.temp_deci_c/10.0
                         <<",\"co2_ppm\":"<<point.co2_ppm<<",\"rh_pct\":"<<point.rh_deci_pct/10.0
                         <<",\"occupied\":"<<(point.occupied?"true":"false")
                         <<",\"hvac\":"<<(point.hvac_on?"true":"false")<<",\"watts\":"<<point.watts
                         <<",\"co2_alarm\":"<<(current_alarm?"true":"false")<<",\"modbus_poll_us\":"<<poll_us<<"}\n";
            }catch(const std::invalid_argument& e){
                ++validation_errors;delay=std::max(delay,backoff.failure());
                std::cerr<<"sensor validation error: "<<e.what()<<'\n';
            }catch(const std::exception& e){
                ++modbus_errors;modbus.disconnect();delay=std::max(delay,backoff.failure());
                std::cerr<<"Modbus error: "<<e.what()<<'\n';
            }
            const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start);
            if(elapsed<std::chrono::milliseconds(delay))
                std::this_thread::sleep_for(std::chrono::milliseconds(delay)-elapsed);
        }
        std::string report="{\"poll_success\":"+std::to_string(successes)+
           ",\"modbus_errors\":"+std::to_string(modbus_errors)+
           ",\"validation_errors\":"+std::to_string(validation_errors)+
           ",\"collector_errors\":"+std::to_string(collector_errors)+
           ",\"alarm_transitions\":"+std::to_string(alarm_edges)+
           ",\"ring_evictions\":"+std::to_string(samples.dropped())+
           ",\"poll_p50_us\":"+std::to_string(bms::percentile_us(latencies,50))+
           ",\"poll_p95_us\":"+std::to_string(bms::percentile_us(latencies,95))+
           ",\"poll_p99_us\":"+std::to_string(bms::percentile_us(latencies,99))+
           ",\"latency_sample_count\":"+std::to_string(latencies.size())+"}";
        std::cerr<<"BMS_METRICS "<<report<<'\n';
        if(!o.metrics_file.empty()){
            std::ofstream out(o.metrics_file);
            if(!out)throw std::runtime_error("cannot open metrics file");
            out<<report<<'\n';
        }
        return successes>0?0:3;
    }catch(const std::exception& e){std::cerr<<"fatal: "<<e.what()<<'\n';return 2;}
}
