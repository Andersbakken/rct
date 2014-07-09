#ifndef CONNECTION_H
#define CONNECTION_H

#include <rct/Buffer.h>
#include <rct/Message.h>
#include <rct/SocketClient.h>
#include <rct/String.h>
#include <rct/LinkedList.h>
#include <rct/Map.h>
#include <rct/ResponseMessage.h>
#include <rct/SignalSlot.h>
#include <rct/ConnectMessage.h>
#include <rct/FinishMessage.h>

class ConnectionPrivate;
class SocketClient;
class Event;
class Connection
{
public:
    Connection();
    Connection(const SocketClient::SharedPtr &client);
    ~Connection();

    void setSilent(bool on) { mSilent = on; }
    bool isSilent() const { return mSilent; }

    bool connectUnix(const Path &socketFile, int timeout = 0);
    bool connectTcp(const String &host, uint16_t port, int timeout = 0);

    int pendingWrite() const;

    bool send(Message&& message);
    bool send(const Message& message);

    template <int StaticBufSize>
    bool write(const char *format, ...)
    {
        if (mSilent)
            return isConnected();
        va_list args;
        va_start(args, format);
        const String ret = String::format<StaticBufSize>(format, args);
        va_end(args);
        return send(ResponseMessage(ret));
    }
    bool write(const String &out)
    {
        if (mSilent)
            return isConnected();
        return send(ResponseMessage(out));
    }

    void finish(int status = 0) { send(FinishMessage(status)); }
    template <int StaticBufSize>
    void finish(const char *format, ...)
    {
        if (!mSilent) {
            va_list args;
            va_start(args, format);
            const String ret = String::format<StaticBufSize>(format, args);
            va_end(args);
            send(ResponseMessage(ret));
        }
        send(FinishMessage(0));
    }

    void finish(const String &msg, int status = 0)
    {
        if (!mSilent)
            send(ResponseMessage(msg));
        send(FinishMessage(status));
    }

    int finishStatus() const { return mFinishStatus; }

    void close() { assert(mSocketClient); mSocketClient->close(); }

    bool isConnected() const { return mSocketClient->isConnected(); }

    Signal<std::function<void(Connection*)> > &sendFinished() { return mSendFinished; }
    Signal<std::function<void(Connection*)> > &connected() { return mConnected; }
    Signal<std::function<void(Connection*)> > &disconnected() { return mDisconnected; }
    Signal<std::function<void(Connection*)> > &error() { return mError; }
    Signal<std::function<void(Connection*, int)> > &finished() { return mFinished; }
    Signal<std::function<void(Message*, Connection*)> > &newMessage() { return mNewMessage; }
    SocketClient::SharedPtr client() const { return mSocketClient; }
    bool send(uint8_t messageId, const String& message);
private:

    void onClientConnected(const SocketClient::SharedPtr&) { mConnected(this); }
    void onClientDisconnected(const SocketClient::SharedPtr&) { mDisconnected(this); }
    void onDataAvailable(const SocketClient::SharedPtr&, Buffer&& buffer);
    void onDataWritten(const SocketClient::SharedPtr&, int);
    void onSocketError(const SocketClient::SharedPtr&, SocketClient::Error error)
    {
        ::warning() << "Socket error" << error;
        mDisconnected(this);
    }
    void initConnection();

    SocketClient::SharedPtr mSocketClient;
    LinkedList<Buffer> mBuffers;
    int mPendingRead, mPendingWrite;
    int mTimeoutTimer, mFinishStatus;

    bool mSilent, mIsConnected, mWarned;

    Signal<std::function<void(Message*, Connection*)> > mNewMessage;
    Signal<std::function<void(Connection*)> > mConnected, mDisconnected, mError, mSendFinished;
    Signal<std::function<void(Connection*, int)> > mFinished;

};

inline bool Connection::send(Message&& message)
{
    String encoded;
    Serializer serializer(encoded);
    message.encode(serializer);
    return send(message.messageId(), encoded);
}

inline bool Connection::send(const Message& message)
{
    String encoded;
    Serializer serializer(encoded);
    message.encode(serializer);
    return send(message.messageId(), encoded);
}

#endif // CONNECTION_H
