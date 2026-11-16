#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include "bms/core.hpp"

namespace bms {
class Connection {
    int fd_=-1;
public:
    Connection()=default;
    Connection(const Connection&)=delete;
    Connection& operator=(const Connection&)=delete;
    ~Connection();
    void connect(const std::string& host,std::uint16_t port,unsigned timeout_ms=1000);
    void close() noexcept;
    bool connected() const noexcept {return fd_>=0;}
    void write_all(const std::uint8_t* data,std::size_t len);
    void read_exact(std::uint8_t* data,std::size_t len);
};
class ModbusClient {
    Connection connection_;
    std::uint16_t tid_=0;
    std::string host_;
    std::uint16_t port_;
public:
    ModbusClient(std::string host,std::uint16_t port):host_(std::move(host)),port_(port){}
    std::vector<std::uint16_t> read_holding(std::uint8_t unit,std::uint16_t start,std::uint16_t count);
    void disconnect() noexcept {connection_.close();}
};
class CollectorClient {
    Connection connection_;
    std::string host_;
    std::uint16_t port_;
public:
    CollectorClient(std::string host,std::uint16_t port):host_(std::move(host)),port_(port){}
    void send(const std::array<std::uint8_t,kFrameSize>& frame);
    void disconnect() noexcept {connection_.close();}
};
}
