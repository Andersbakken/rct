#ifndef SOCKETCLIENT_H
#define SOCKETCLIENT_H

#include <rct/String.h>
#include <rct/EventReceiver.h>
#include <rct/SignalSlot.h>
#include <rct/rct-config.h>
#include <deque>

class SocketClient : public EventReceiver
{
public:
    SocketClient();
    virtual ~SocketClient();

    bool connect(const Path& path, int maxTime = -1);
    void disconnect();

    bool isConnected() const { return mFd != -1; }

    String readAll();
    int read(char *buf, int size);
    int bytesAvailable() const { return mReadBuffer.size() - mReadBufferPos; }
    bool write(const String& data);

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

    std::deque<String> mBuffers;
    int mBufferIdx;
    String mReadBuffer;
    int mReadBufferPos;
};

#endif
