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
class Connection : public std::enable_shared_from_this<Connection>
{
public:
    Connection();
    Connection(const SocketClient::SharedPtr &client);
    virtual ~Connection();

    void setSilent(bool on) { mSilent = on; }
    bool isSilent() const { return mSilent; }

    bool connectUnix(const Path &socketFile, int timeout = 0);
    bool connectTcp(const String &host, uint16_t port, int timeout = 0);

    int pendingWrite() const;

    virtual bool send(const Message &message)
    {
        // ::error() << getpid() << "sending message" << static_cast<int>(id) << message.size();
        if (!mSocketClient->isConnected()) {
            if (!mWarned) {
                mWarned = true;
                ::error("Trying to send message to unconnected client (%d)", message.messageId());
            }
            return false;
        }

        String header, value;
        message.prepare(header, value);
        mPendingWrite += header.size() + value.size();
        return mSocketClient->write(header) && (value.isEmpty() || mSocketClient->write(value));
    }

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
    Signal<std::function<void(std::shared_ptr<Message>, Connection*)> > &newMessage() { return mNewMessage; }
    SocketClient::SharedPtr client() const { return mSocketClient; }
private:

    void onClientConnected(const SocketClient::SharedPtr&) { mConnected(this); }
    void onClientDisconnected(const SocketClient::SharedPtr&) { mDisconnected(this); }
    void onDataAvailable(const SocketClient::SharedPtr&, Buffer&& buffer);
    void onDataWritten(const SocketClient::SharedPtr&, int);
    void onSocketError(const SocketClient::SharedPtr&, SocketClient::Error error)
    {
        ::warning() << "Socket error" << error << errno << strerror(errno);
        mDisconnected(this);
    }
    void checkData();

    SocketClient::SharedPtr mSocketClient;
    LinkedList<Buffer> mBuffers;
    int mPendingRead, mPendingWrite;
    int mTimeoutTimer, mFinishStatus;

    bool mSilent, mIsConnected, mWarned;

    Signal<std::function<void(std::shared_ptr<Message>, Connection*)> > mNewMessage;
    Signal<std::function<void(Connection*)> > mConnected, mDisconnected, mError, mSendFinished;
    Signal<std::function<void(Connection*, int)> > mFinished;

};

#endif // CONNECTION_H
