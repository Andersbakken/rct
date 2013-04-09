#include "rct/Connection.h"
#include "rct/SocketClient.h"
#include "rct/Event.h"
#include "rct/EventLoop.h"
#include "rct/Serializer.h"
#include "rct/Messages.h"

class ResponseMessageEvent : public Event
{
public:
    enum { Type = 1 };
    ResponseMessageEvent(const String &r)
        : Event(Type), response(r)
    {}

    const String response;
};

Connection::Connection()
    : mClient(new SocketClient), mPendingRead(0), mPendingWrite(0), mDone(false), mSilent(false)
{
    mClient->connected().connect(this, &Connection::onClientConnected);
    mClient->disconnected().connect(this, &Connection::onClientDisconnected);
    mClient->dataAvailable().connect(this, &Connection::dataAvailable);
    mClient->bytesWritten().connect(this, &Connection::dataWritten);
}

Connection::Connection(SocketClient* client)
    : mClient(client), mPendingRead(0), mPendingWrite(0), mDone(false), mSilent(false)
{
    assert(client->isConnected());
    mClient->disconnected().connect(this, &Connection::onClientDisconnected);
    mClient->dataAvailable().connect(this, &Connection::dataAvailable);
    mClient->bytesWritten().connect(this, &Connection::dataWritten);
}

Connection::~Connection()
{
    mDestroyed(this);
    delete mClient;
}


bool Connection::connectToServer(const String &name, int timeout)
{
    return mClient->connectUnix(name, timeout);
}

bool Connection::send(int id, const String &message)
{
    if (message.isEmpty())
        return true;

    if (!mClient->isConnected()) {
        ::error("Trying to send message to unconnected client (%d)", id);
        return false;
    }

    if (mSilent)
        return true;

    String header, data;
    {
        if (message.size()) {
            Serializer strm(data);
            strm << id;
            strm.write(message.constData(), message.size());
        }
        Serializer strm(header);
        strm << data.size();
    }
    mPendingWrite += (header.size() + data.size());
    return mClient->write(header) && mClient->write(data);
}

int Connection::pendingWrite() const
{
    return mPendingWrite;
}

void Connection::finish()
{
    mDone = true;
    dataWritten(mClient, 0);
}

void Connection::dataAvailable(SocketClient *)
{
    while (true) {
        int available = mClient->bytesAvailable();
        assert(available >= 0);
        if (!mPendingRead) {
            if (available < static_cast<int>(sizeof(uint32_t)))
                break;
            char buf[sizeof(uint32_t)];
            const int read = mClient->read(buf, 4);
            assert(read == 4);
            Deserializer strm(buf, read);
            strm >> mPendingRead;
            available -= 4;
        }
        if (available < mPendingRead)
            break;
        char buf[1024];
        char *buffer = buf;
        if (mPendingRead > static_cast<int>(sizeof(buf))) {
            buffer = new char[mPendingRead];
        }
        const int read = mClient->read(buffer, mPendingRead);
        assert(read == mPendingRead);
        Message *message = Messages::create(buffer, read);
        if (message) {
            newMessage()(message, this);
            delete message;
        }
        if (buffer != buf)
            delete[] buffer;

        mPendingRead = 0;
        // mClient->dataAvailable().disconnect(this, &Connection::dataAvailable);
    }
}

void Connection::dataWritten(SocketClient *, int bytes)
{
    assert(mPendingWrite >= bytes);
    mPendingWrite -= bytes;
    if (!mPendingWrite) {
        if (bytes)
            mSendComplete(this);
        if (mDone) {
            mClient->disconnect();
            deleteLater();
        }
    }
}

void Connection::writeAsync(const String &out)
{
    EventLoop::instance()->postEvent(this, new ResponseMessageEvent(out));
}

void Connection::event(const Event *e)
{
    switch (e->type()) {
    case ResponseMessageEvent::Type: {
        ResponseMessage msg(static_cast<const ResponseMessageEvent*>(e)->response);
        send(&msg);
        break; }
    default:
        EventReceiver::event(e);
    }
}
