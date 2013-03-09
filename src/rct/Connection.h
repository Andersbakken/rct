#ifndef CONNECTION_H
#define CONNECTION_H

#include <rct/Message.h>
#include <rct/EventReceiver.h>
#include <rct/LocalClient.h>
#include <rct/String.h>
#include <rct/Map.h>
#include <rct/ResponseMessage.h>
#include <rct/SignalSlot.h>

class ConnectionPrivate;
class LocalClient;
class Event;
class Connection : public EventReceiver
{
public:
    Connection();
    Connection(LocalClient *client);
    ~Connection();

    void setSilent(bool on) { mSilent = on; }
    bool isSilent() const { return mSilent; }

    bool connectToServer(const String &name, int timeout);

    int pendingWrite() const;

    template<typename T> bool send(const T *message);
    bool send(int id, const String& message);
    template <int StaticBufSize>
    bool write(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        const String ret = String::format<StaticBufSize>(format, args);
        va_end(args);
        ResponseMessage msg(ret);
        return send(&msg);
    }
    bool write(const String &out)
    {
        ResponseMessage msg(out);
        return send(&msg);
    }

    void writeAsync(const String &out);
    void finish();

    bool isConnected() const { return mClient->isConnected(); }

    signalslot::Signal1<Connection*> &connected() { return mConnected; }
    signalslot::Signal1<Connection*> &disconnected() { return mDisconnected; }
    signalslot::Signal1<Connection*> &error() { return mError; }
    signalslot::Signal2<Message*, Connection*> &newMessage() { return mNewMessage; }
    signalslot::Signal1<Connection*> &sendComplete() { return mSendComplete; }
    signalslot::Signal1<Connection*> &destroyed() { return mDestroyed; }

    LocalClient *client() const { return mClient; }
protected:
    void event(const Event *e);
private:
    void onClientConnected(LocalClient *) { mConnected(this); }
    void onClientDisconnected(LocalClient *) { mDisconnected(this); }
    void dataAvailable(LocalClient *);
    void dataWritten(LocalClient *, int bytes);

    LocalClient *mClient;
    int mPendingRead, mPendingWrite;
    bool mDone, mSilent;

    signalslot::Signal2<Message*, Connection*> mNewMessage;
    signalslot::Signal1<Connection*> mDestroyed, mConnected, mDisconnected, mSendComplete, mError;
};

template<typename T>
bool Connection::send(const T *message)
{
    return send(message->messageId(), message->encode());
}

#endif // CONNECTION_H
