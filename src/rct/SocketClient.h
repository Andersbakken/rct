#ifndef SOCKETCLIENT_H
#define SOCKETCLIENT_H

#include <rct/String.h>
#include <rct/EventReceiver.h>
#include <rct/SignalSlot.h>
#include <rct/rct-config.h>
#include <deque>
#include <netinet/in.h>

class SocketClient : public EventReceiver
{
public:
    SocketClient();
    virtual ~SocketClient();

    bool connectUnix(const Path& path, int maxTime = -1);
    bool connectTcp(const String& host, uint16_t port, int maxTime = -1);
    void disconnect();

    bool isConnected() const { return mFd != -1; }

    bool receiveFrom(uint16_t port);
    bool receiveFrom(const String& ip, uint16_t port);
    bool addMulticast(const String& multicast, const String& interface = String());
    void removeMulticast(const String& multicast);
    void setMulticastLoop(bool loop = true);

    String readAll();
    int read(char *buf, int size);
    int bytesAvailable() const { return mReadBuffer.size() - mReadBufferPos; }
    bool write(const String& data);
    bool writeTo(const String& host, uint16_t port, const String& data);

    signalslot::Signal1<SocketClient*> &dataAvailable() { return mDataAvailable; }
    signalslot::Signal1<SocketClient*> &connected() { return mConnected; }
    signalslot::Signal1<SocketClient*> &disconnected() { return mDisconnected; }
    signalslot::Signal2<SocketClient*, int>& bytesWritten() { return mBytesWritten; }
protected:
    virtual void event(const Event* event);
private:
    static void dataCallback(int fd, unsigned int flags, void* userData);

    bool writeMore();
    void readMore();
    SocketClient(int fd);
    friend class SocketServer;
    int mFd;
    signalslot::Signal1<SocketClient*> mDataAvailable, mConnected, mDisconnected;
    signalslot::Signal2<SocketClient*, int> mBytesWritten;

    std::deque<std::pair<sockaddr_in, String> > mBuffers;
    int mBufferIdx;
    String mReadBuffer;
    int mReadBufferPos;
};

#endif
