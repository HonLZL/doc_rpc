#ifndef ROCKET_NET_TCP_NET_ADDR_H
#define ROCKET_NET_TCP_NET_ADDR_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <memory>
#include <string>

namespace rocket {
class NetAddr {
   public:
    typedef std::shared_ptr<NetAddr> s_ptr;
    // 纯虚函数,支持多态,子类必须重写这些纯虚函数
    virtual sockaddr* getSockAddr() = 0;
    virtual socklen_t getSockLen() = 0;
    virtual int getFamily() = 0;
    virtual std::string toString() = 0;
    virtual bool checkValid() = 0;
};

class IPNetAddr : public NetAddr {
   public:
    IPNetAddr(const std::string& ip, uint16_t port);  // 由字符串ip 和 无符号16位整数 范围为 [0, 2^16-1=65535] 构造
    IPNetAddr(const std::string& addr);               // 由字符串构造 
    IPNetAddr(sockaddr_in addr);                      // 直接由  IPv4 地址信息的结构体构造

    sockaddr* getSockAddr();
    socklen_t getSockLen();
    int getFamily();
    std::string toString();
    bool checkValid();  // 判断 ip 和 port 是否合法

   private:
    std::string m_ip;
    uint16_t m_port{0};
    sockaddr_in m_addr;
};
}  // namespace rocket

#endif