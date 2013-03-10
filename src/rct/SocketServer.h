#ifndef SOCKETSERVER_H
#define SOCKETSERVER_H

#include <rct/Path.h>
#include <rct/SignalSlot.h>
#include <deque>

class SocketClient;

class SocketServer
{
public:
    SocketServer();
    ~SocketServer();

    bool listen(const Path& path);

    SocketClient* nextClient();

    signalslot::Signal0& clientConnected() { return mClientConnected; }

private:
    static void listenCallback(int fd, unsigned int flags, void* userData);

private:
    int mFd;
    std::deque<int> mPendingClients;
    signalslot::Signal0 mClientConnected;
};

#endif
