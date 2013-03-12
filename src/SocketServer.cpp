#include "rct/SocketServer.h"
#include "rct/EventLoop.h"
#include "rct/SocketClient.h"
#include "rct/Log.h"
#include "rct/Rct.h"
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#define LISTEN_BACKLOG 5

SocketServer::SocketServer()
    : mMode(SocketClient::Unix), mFd(-1)
{
}

SocketServer::~SocketServer()
{
    if (mFd != -1) {
        EventLoop::instance()->removeFileDescriptor(mFd);
        int ret;
        eintrwrap(ret, ::close(mFd));
    }
}

static inline bool listenInternal(int& fd, sockaddr* address, size_t addressSize)
{
    if (bind(fd, address, addressSize) != 0) {
        int ret;
        eintrwrap(ret, ::close(fd));
        fd = -1;
        error("SocketServer::listen() Unable to bind");
        return false;
    }

    if (listen(fd, LISTEN_BACKLOG) != 0) {
        int ret;
        eintrwrap(ret, ::close(fd));
        error("SocketServer::listen() Unable to listen to socket");
        fd = -1;
        return false;
    }

    return true;
}

bool SocketServer::listenUnix(const Path& path)
{
    mMode = SocketClient::Unix;

    if (path.exists()) {
        return false;
    }
    sockaddr_un address;

    mFd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (mFd < 0) {
        error("SocketServer::listenUnix() Unable to create socket");
        return false;
    }

    memset(&address, 0, sizeof(sockaddr_un));

    if (static_cast<int>(sizeof(address.sun_path)) - 1 <= path.size()) {
        int ret;
        eintrwrap(ret, ::close(mFd));
        mFd = -1;
        error("SocketServer::listenUnix() Path too long %s", path.constData());
        return false;
    }

    address.sun_family = AF_UNIX;
    memcpy(address.sun_path, path.nullTerminated(), path.size() + 1);

    if (listenInternal(mFd, reinterpret_cast<sockaddr*>(&address), sizeof(sockaddr_un))) {
        EventLoop::instance()->addFileDescriptor(mFd, EventLoop::Read, listenCallback, this);
        return true;
    }
    return false;
}

bool SocketServer::listenTcp(uint16_t port)
{
    mMode = SocketClient::Tcp;

    sockaddr_in address;

    mFd = socket(AF_INET, SOCK_STREAM, 0);
    if (mFd < 0) {
        error("SocketServer::listenTcp() Unable to create socket");
        return false;
    }

    memset(&address, 0, sizeof(sockaddr_in));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (listenInternal(mFd, reinterpret_cast<sockaddr*>(&address), sizeof(sockaddr_in))) {
        EventLoop::instance()->addFileDescriptor(mFd, EventLoop::Read, listenCallback, this);
        return true;
    }
    return false;
}

bool SocketServer::listenTcp(const String& ip, uint16_t port)
{
    mMode = SocketClient::Tcp;

    sockaddr_in address;

    mFd = socket(AF_INET, SOCK_STREAM, 0);
    if (mFd < 0) {
        error("SocketServer::listenTcp() Unable to create socket");
        return false;
    }

    memset(&address, 0, sizeof(sockaddr_in));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip.nullTerminated());
    address.sin_port = htons(port);

    if (listenInternal(mFd, reinterpret_cast<sockaddr*>(&address), sizeof(sockaddr_in))) {
        EventLoop::instance()->addFileDescriptor(mFd, EventLoop::Read, listenCallback, this);
        return true;
    }
    return false;
}

void SocketServer::listenCallback(int, unsigned int, void* userData)
{
    SocketServer* server = reinterpret_cast<SocketServer*>(userData);

    int clientFd;
    eintrwrap(clientFd, ::accept(server->mFd, NULL, NULL));
    if (clientFd != -1) {
        server->mPendingClients.push_back(clientFd);
        server->mClientConnected();
    }
}

SocketClient* SocketServer::nextClient()
{
    if (mPendingClients.empty())
        return 0;
    const int clientFd = mPendingClients.front();
    mPendingClients.pop_front();
    return new SocketClient(mMode, clientFd);
}
