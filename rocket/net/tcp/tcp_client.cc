#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../../common/error_code.h"
#include "../../common/log.h"
#include "../eventloop.h"
#include "../fd_event_group.h"
#include "net_addr.h"
#include "tcp_client.h"
#include "tcp_connection.h"
#include "../timer.h"

namespace rocket {
TcpClient::TcpClient(NetAddr::s_ptr peer_addr)
    : m_peer_addr(peer_addr) {
    m_event_loop = EventLoop::GetCurrentEventLoop();
    m_fd = socket(peer_addr->getFamily(), SOCK_STREAM, 0);

    if (m_fd < 0) {
        ERRORLOG("TcpClient::TcpClient() error, failed to create fa");
        return;
    }
    m_fd_event = FdEventGroup::GetFdEventGroup()->getFdEvent(m_fd);
    m_fd_event->setNonBlock();

    m_connection = std::make_shared<TcpConnection>(m_event_loop, m_fd, 128, peer_addr, nullptr, TcpConnectionByClient);
    m_connection->setConnectionType(TcpConnectionByClient);
}
TcpClient::~TcpClient() {
    DEBUGLOG("TcpClient::~TcpClient()");
    if (m_fd > 0) {
        close(m_fd);
    }
}

// 异步进行connect
// 如果connect完成, done 会被执行
void TcpClient::connect(std::function<void()> done) {
    int rt = ::connect(m_fd, m_peer_addr->getSockAddr(), m_peer_addr->getSockLen());
    if (rt == 0) {  // 连接成功
        DEBUGLOG("connect [%s] successfully", m_peer_addr->toString().c_str());
        m_connection->setState(Connected);
        initLocalAddr();
        if (done) {
            done();
        }
    } else if (rt == -1) {
        if (errno == EINPROGRESS) {
            // EINPROGRESS 错误码表明某些套接字操作（通常是非阻塞连接）仍在进行中，尚未完成
            // epoll 监听可写事件,然后判断错误码
            m_fd_event->listen(FdEvent::OUT_EVENT,
                               [this, done]() {
                                   int rt = ::connect(m_fd, m_peer_addr->getSockAddr(), m_peer_addr->getSockLen());
                                   if ((rt < 0 && errno == EISCONN) || (rt == 0)) {
                                       DEBUGLOG("connect [%s] sussess", m_peer_addr->toString().c_str());
                                       initLocalAddr();
                                       m_connection->setState(Connected);
                                   } else {
                                       if (errno == ECONNREFUSED) {
                                           m_connect_error_code = ERROR_PEER_CLOSED;
                                           m_connect_error_info = "connect refused, sys error = " + std::string(strerror(errno));
                                       } else {
                                           m_connect_error_code = ERROR_FAILED_CONNECT;
                                           m_connect_error_info = "connect unkonwn error, sys error = " + std::string(strerror(errno));
                                       }
                                       ERRORLOG("connect errror, errno=%d, error=%s", errno, strerror(errno));
                                       close(m_fd);
                                       m_fd = socket(m_peer_addr->getFamily(), SOCK_STREAM, 0);
                                   }

                                   // 连接完后需要去掉可写事件的监听，不然会一直触发
                                   m_event_loop->deleteEpollEvent(m_fd_event);
                                   DEBUGLOG("now begin to done");
                                   // 如果连接完成，才会执行回调函数
                                   if (done) {
                                       done();
                                   }
                               });
            m_event_loop->addEpollEvent(m_fd_event);

            if (!m_event_loop->isLooping()) {
                m_event_loop->loop();
            }
        } else {
            ERRORLOG("connect errror, errno=%d, error=%s", errno, strerror(errno));
            m_connect_error_code = ERROR_FAILED_CONNECT;
            m_connect_error_info = "connect error, sys error = " + std::string(strerror(errno));
            if (done) {
                done();
            }
        }
    }
}

// 异步发送 Message, 字符串 或 RPC 协议,发送成功,会调用 done 函数,函数的入参就是 message 对象
void TcpClient::writeMessage(AbstractProtocol::s_ptr message, std::function<void(AbstractProtocol::s_ptr)> done) {
    // 1 把 message 对象写入到 Connection 的 buffer,done也写入进去
    // 2　启动　connection 可写事件
    m_connection->pushSendMessage(message, done);
    m_connection->listenWrite();
}

// 　异步读取　message 成功，会调用　done 函数，函数的入参是　message 对象
void TcpClient::readMessage(const std::string& msg_id, std::function<void(AbstractProtocol::s_ptr)> done) {
    // １　监听可读事件
    // ２　从 buffer 里　decode 得到　message 对象, 判断　msg_id 是否相等，相等则读成功，执行其回调函数
    m_connection->pushReadMessage(msg_id, done);
    m_connection->listenRead();
}

void TcpClient::stop() {
    if (!m_event_loop->isLooping()) {
        m_event_loop->stop();
    }
}

int TcpClient::getConnectErrorCode() {
    return m_connect_error_code;
}

std::string TcpClient::getConnectErrorInfo() {
    return m_connect_error_info;
}

NetAddr::s_ptr TcpClient::getPeerAddr() {
    return m_peer_addr;
}
NetAddr::s_ptr TcpClient::getLocalAddr() {
    return m_local_addr;
}

void TcpClient::initLocalAddr() {
    sockaddr_in local_addr;
    socklen_t len = sizeof(local_addr);
    int ret = getsockname(m_fd, reinterpret_cast<sockaddr*>(&local_addr), &len);
    if (ret != 0) {
        ERRORLOG("initLocalAddr error, getsockname error, errno=%d, error=%s", errno, strerror(errno));
        return;
    }
    m_local_addr = std::make_shared<IPNetAddr>(local_addr);
}

void TcpClient::addTimerEvent(TimerEvent::s_ptr timer_event) {
    m_event_loop->addTimerEvent(timer_event);
}

}  // namespace rocket
