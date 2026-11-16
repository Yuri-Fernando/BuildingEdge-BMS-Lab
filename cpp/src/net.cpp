#include "bms/net.hpp"
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/time.h>
#include <unistd.h>

namespace bms {
namespace {
void put16(std::uint8_t* b,std::uint16_t v){b[0]=static_cast<std::uint8_t>(v>>8);b[1]=static_cast<std::uint8_t>(v);}
std::uint16_t get16(const std::uint8_t* b){return static_cast<std::uint16_t>((b[0]<<8)|b[1]);}
}
Connection::~Connection(){close();}
void Connection::close() noexcept {if(fd_>=0){::close(fd_);fd_=-1;}}
void Connection::connect(const std::string& host,std::uint16_t port,unsigned timeout_ms){
    close();
    addrinfo hints{}; hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;
    addrinfo* addresses=nullptr;
    const auto port_str=std::to_string(port);
    if(::getaddrinfo(host.c_str(),port_str.c_str(),&hints,&addresses)!=0)throw std::runtime_error("DNS lookup failed for "+host);
    bool connected=false;
    for(addrinfo* p=addresses;p;p=p->ai_next){
        int s=::socket(p->ai_family,p->ai_socktype,p->ai_protocol);
        if(s<0)continue;
        timeval tv{};tv.tv_sec=timeout_ms/1000;tv.tv_usec=static_cast<suseconds_t>(timeout_ms%1000)*1000;
        ::setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
        ::setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv));
        if(::connect(s,p->ai_addr,p->ai_addrlen)==0){fd_=s;connected=true;break;}
        ::close(s);
    }
    ::freeaddrinfo(addresses);
    if(!connected)throw std::runtime_error("cannot connect to "+host+":"+port_str);
}
void Connection::write_all(const std::uint8_t* data,std::size_t size){
    std::size_t n=0;
    while(n<size){auto k=::send(fd_,data+n,size-n,MSG_NOSIGNAL);if(k<=0){close();throw std::runtime_error("socket send failure");}n+=static_cast<std::size_t>(k);}
}
void Connection::read_exact(std::uint8_t* data,std::size_t size){
    std::size_t n=0;
    while(n<size){auto k=::recv(fd_,data+n,size-n,0);if(k<=0){close();throw std::runtime_error("socket receive failure/timeout");}n+=static_cast<std::size_t>(k);}
}
std::vector<std::uint16_t> ModbusClient::read_holding(std::uint8_t unit,std::uint16_t start,std::uint16_t count){
    if(count==0||count>125)throw std::invalid_argument("Modbus read count out of range");
    try{
        if(!connection_.connected())connection_.connect(host_,port_);
        const std::uint16_t tid=++tid_;
        std::uint8_t request[12]{};
        put16(request,tid);put16(request+4,6);request[6]=unit;request[7]=3;put16(request+8,start);put16(request+10,count);
        connection_.write_all(request,sizeof(request));
        std::uint8_t hdr[7]{};connection_.read_exact(hdr,sizeof(hdr));
        const auto length=get16(hdr+4);
        if(length<3||length>253)throw std::runtime_error("Modbus MBAP payload limit");
        std::vector<std::uint8_t> response(hdr,hdr+7);response.resize(6+length);
        connection_.read_exact(response.data()+7,length-1);
        return decode_modbus_reply(response,tid,unit,count);
    }catch(...){connection_.close();throw;}
}
void CollectorClient::send(const std::array<std::uint8_t,kFrameSize>& frame){
    try{if(!connection_.connected())connection_.connect(host_,port_);
        connection_.write_all(frame.data(),frame.size());
    }catch(...){connection_.close();throw;}
}
}
