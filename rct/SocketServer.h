#ifndef SOCKETSERVER_H
#define SOCKETSERVER_H

#include <rct/Path.h>
#include <rct/SignalSlot.h>
#include <rct/SocketClient.h>
#include <deque>

class SocketClient;

class SocketServer
{
public:
    SocketServer();
    ~SocketServer();

    bool listenUnix(const Path& path);
    bool listenTcp(uint16_t port);
    bool listenTcp(const String& ip, uint16_t port);

    SocketClient* nextClient();

    signalslot::Signal0& clientConnected() { return mClientConnected; }

private:
    static void listenCallback(int fd, unsigned int flags, void* userData);

private:
    SocketClient::Mode mMode;

    int mFd;
    std::deque<int> mPendingClients;
    signalslot::Signal0 mClientConnected;
};

#endif
