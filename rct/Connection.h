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
#include <rct/FinishMessage.h>

class ConnectionPrivate;
class SocketClient;
class Event;
class Connection
{
public:
    Connection();
    Connection(const SocketClient::SharedPtr &client);

    void setSilent(bool on) { mSilent = on; }
    bool isSilent() const { return mSilent; }

    bool connectToServer(const String &name, int timeout);

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

    void writeAsync(const String &out);
    void finish() { send(FinishMessage()); }

    bool isConnected() const { return mSocketClient->isConnected(); }

    Signal<std::function<void(Connection*)> > &connected() { return mConnected; }
    Signal<std::function<void(Connection*)> > &disconnected() { return mDisconnected; }
    Signal<std::function<void(Connection*)> > &error() { return mError; }
    Signal<std::function<void(Connection*)> > &finished() { return mFinished; }
    Signal<std::function<void(Message*, Connection*)> > &newMessage() { return mNewMessage; }
    SocketClient::SharedPtr client() const { return mSocketClient; }

private:
    bool sendData(uint8_t id, const String& message);

    void onClientConnected(const SocketClient::SharedPtr&) { mConnected(this); }
    void onClientDisconnected(const SocketClient::SharedPtr&) { mDisconnected(this); }
    void onDataAvailable(SocketClient::SharedPtr&);
    void onDataWritten(const SocketClient::SharedPtr&, int);
    void onSocketError(const SocketClient::SharedPtr&, SocketClient::Error) { mDisconnected(this); }
    void checkData();

    SocketClient::SharedPtr mSocketClient;
    LinkedList<Buffer> mBuffers;
    int mPendingRead, mPendingWrite;
    bool mSilent;

    Signal<std::function<void(Message*, Connection*)> > mNewMessage;
    Signal<std::function<void(Connection*)> > mConnected, mDisconnected, mError, mFinished;
};

inline bool Connection::send(Message&& message)
{
    String encoded;
    Serializer serializer(encoded);
    message.encode(serializer);
    return sendData(message.messageId(), encoded);
}

inline bool Connection::send(const Message& message)
{
    String encoded;
    Serializer serializer(encoded);
    message.encode(serializer);
    return sendData(message.messageId(), encoded);
}

#endif // CONNECTION_H
