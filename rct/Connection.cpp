#include "Connection.h"
#include "EventLoop.h"
#include "Serializer.h"
#include "Message.h"
#include "Timer.h"
#include <assert.h>

#include "Connection.h"

Connection::Connection(int version)
    : mPendingRead(0), mPendingWrite(0), mTimeoutTimer(0), mFinishStatus(0),
      mVersion(version), mSilent(false), mIsConnected(false), mWarned(false)
{
}

Connection::Connection(const SocketClient::SharedPtr &client, int version)
    : mSocketClient(client), mPendingRead(0), mPendingWrite(0), mTimeoutTimer(0), mFinishStatus(0),
      mVersion(version), mSilent(false), mIsConnected(true), mWarned(false)
{
    assert(client->isConnected());
    mSocketClient->disconnected().connect(std::bind(&Connection::onClientDisconnected, this, std::placeholders::_1));
    mSocketClient->readyRead().connect(std::bind(&Connection::onDataAvailable, this, std::placeholders::_1, std::placeholders::_2));
    mSocketClient->bytesWritten().connect(std::bind(&Connection::onDataWritten, this, std::placeholders::_1, std::placeholders::_2));
    mSocketClient->error().connect(std::bind(&Connection::onSocketError, this, std::placeholders::_1, std::placeholders::_2));
    send(ConnectMessage());
    EventLoop::eventLoop()->callLater(std::bind(&Connection::checkData, this));
}

Connection::~Connection()
{
    if (mTimeoutTimer) {
        EventLoop::eventLoop()->unregisterTimer(mTimeoutTimer);
    }
}

void Connection::checkData()
{
    if (!mSocketClient->buffer().isEmpty())
        onDataAvailable(mSocketClient, std::forward<Buffer>(mSocketClient->takeBuffer()));
}

bool Connection::connectUnix(const Path &socketFile, int timeout)
{
    if (timeout > 0) {
        mTimeoutTimer = EventLoop::eventLoop()->registerTimer([&](int) {
                if (!mIsConnected) {
                    mSocketClient.reset();
                    mDisconnected(this);
                }
                mTimeoutTimer = 0;
        }, timeout, Timer::SingleShot);
    }
    mSocketClient.reset(new SocketClient);
    mSocketClient->connected().connect(std::bind(&Connection::onClientConnected, this, std::placeholders::_1));
    mSocketClient->disconnected().connect(std::bind(&Connection::onClientDisconnected, this, std::placeholders::_1));
    mSocketClient->readyRead().connect(std::bind(&Connection::onDataAvailable, this, std::placeholders::_1, std::placeholders::_2));
    mSocketClient->bytesWritten().connect(std::bind(&Connection::onDataWritten, this, std::placeholders::_1, std::placeholders::_2));
    mSocketClient->error().connect(std::bind(&Connection::onSocketError, this, std::placeholders::_1, std::placeholders::_2));
    if (!mSocketClient->connect(socketFile)) {
        mSocketClient.reset();
        return false;
    }
    return true;
}

bool Connection::connectTcp(const String &host, uint16_t port, int timeout)
{
    if (timeout > 0) {
        mTimeoutTimer = EventLoop::eventLoop()->registerTimer([&](int) {
                if (!mIsConnected) {
                    mSocketClient.reset();
                    mDisconnected(this);
                }
                mTimeoutTimer = 0;
        }, timeout, Timer::SingleShot);
    }
    mSocketClient.reset(new SocketClient);
    mSocketClient->connected().connect(std::bind(&Connection::onClientConnected, this, std::placeholders::_1));
    mSocketClient->disconnected().connect(std::bind(&Connection::onClientDisconnected, this, std::placeholders::_1));
    mSocketClient->readyRead().connect(std::bind(&Connection::onDataAvailable, this, std::placeholders::_1, std::placeholders::_2));
    mSocketClient->bytesWritten().connect(std::bind(&Connection::onDataWritten, this, std::placeholders::_1, std::placeholders::_2));
    mSocketClient->error().connect(std::bind(&Connection::onSocketError, this, std::placeholders::_1, std::placeholders::_2));
    if (!mSocketClient->connect(host, port)) {
        mSocketClient.reset();
        return false;
    }
    return true;
}

int Connection::pendingWrite() const
{
    return mPendingWrite;
}

static inline unsigned int bufferSize(const LinkedList<Buffer>& buffers)
{
    unsigned int sz = 0;
    for (const Buffer& buffer: buffers) {
        sz += buffer.size();
    }
    return sz;
}

static inline int bufferRead(LinkedList<Buffer>& buffers, char* out, unsigned int size)
{
    if (!size)
        return 0;
    unsigned int num = 0, rem = size, cur;
    LinkedList<Buffer>::iterator it = buffers.begin();
    while (it != buffers.end()) {
        cur = std::min(it->size(), rem);
        memcpy(out + num, it->data(), cur);
        rem -= cur;
        num += cur;
        if (cur == it->size()) {
            // we've read the entire buffer, remove it
            it = buffers.erase(it);
        } else {
            assert(!rem);
            assert(it->size() > cur);
            assert(cur > 0);
            // we need to shrink & memmove the front buffer at this point
            Buffer& front = *it;
            memmove(front.data(), front.data() + cur, front.size() - cur);
            front.resize(front.size() - cur);
        }
        if (!rem) {
            assert(num == size);
            return size;
        }
        assert(rem > 0);
    }
    return num;
}

void Connection::onDataAvailable(const SocketClient::SharedPtr&, Buffer&& buf)
{
    while (true) {
        if (!buf.isEmpty())
            mBuffers.push_back(std::move(buf));

        unsigned int available = bufferSize(mBuffers);
        if (!available)
            break;
        if (!mPendingRead) {
            if (available < static_cast<int>(sizeof(uint32_t)))
                break;
            char buf[sizeof(uint32_t)];
            const int read = bufferRead(mBuffers, buf, 4);
            assert(read == 4);
            Deserializer strm(buf, read);
            strm >> mPendingRead;
            assert(mPendingRead > 0);
            available -= 4;
        }
        assert(mPendingRead >= 0);
        if (available < static_cast<unsigned int>(mPendingRead))
            break;
        char buf[1024];
        char *buffer = buf;
        if (mPendingRead > static_cast<int>(sizeof(buf))) {
            buffer = new char[mPendingRead];
        }
        const int read = bufferRead(mBuffers, buffer, mPendingRead);
        assert(read == mPendingRead);
        mPendingRead = 0;
        std::shared_ptr<Message> message = Message::create(mVersion, buffer, read);
        if (message) {
            if (message->messageId() == FinishMessage::MessageId) {
                mFinishStatus = std::static_pointer_cast<FinishMessage>(message)->status();
                mFinished(this, mFinishStatus);
            } else if (message->messageId() == ConnectMessage::MessageId) {
                mIsConnected = true;
            } else {
                newMessage()(message, this);
            }
        }
        if (buffer != buf)
            delete[] buffer;
        if (!message)
            mSocketClient->close();
    // mClient->dataAvailable().disconnect(this, &Connection::dataAvailable);
    }
}

void Connection::onDataWritten(const SocketClient::SharedPtr&, int bytes)
{
    assert(mPendingWrite >= bytes);
    mPendingWrite -= bytes;
    // ::error() << "wrote some bytes" << mPendingWrite << bytes;
    if (!mPendingWrite) {
        mSendFinished(this);
    }
}

class SocketClientBuffer : public Serializer::Buffer
{
public:
    SocketClientBuffer(const SocketClient::SharedPtr &client)
        : mClient(client), mWritten(0)
    {}

    virtual bool write(const char *data, int len)
    {
        if (mClient->write(data, len)) {
            mWritten += len;
            return true;
        }
        return false;
    }

    virtual int pos() const
    {
        return mWritten;
    }
private:
    SocketClient::SharedPtr mClient;
    int mWritten;
};

bool Connection::send(const Message &message)
{
    // ::error() << getpid() << "sending message" << static_cast<int>(id) << message.size();
    if (!mSocketClient->isConnected()) {
        if (!mWarned) {
            mWarned = true;
            ::error("Trying to send message to unconnected client (%d)", message.messageId());
        }
        return false;
    }

    const int size = message.encodedSize();
    if (size == -1 || message.mFlags & Message::MessageCache) {
        String header, value;
        message.prepare(mVersion, header, value);
        mPendingWrite += header.size() + value.size();
        assert(!size || size == (header.size() + value.size() - 4));
        return (mSocketClient->write(header) && (value.isEmpty() || mSocketClient->write(value)));
    } else {
        assert(size >= 0);
        mPendingWrite += (size + Message::HeaderExtra);
        Serializer serializer(std::unique_ptr<SocketClientBuffer>(new SocketClientBuffer(mSocketClient)));
        message.encodeHeader(serializer, size, mVersion);
        message.encode(serializer);
        assert(serializer.hasError() || serializer.pos() == size + 4);
        return !serializer.hasError();
    }
}

