#include "Connection.h"

#include <assert.h>
#include <stddef.h>
#include <utility>

#include "EventLoop.h"
#include "Message.h"
#include "Serializer.h"
#include "StackBuffer.h"
#include "Timer.h"
#include "rct/FinishMessage.h"
#include "rct/SocketClient.h"
#include "rct/String.h"

Connection::Connection(int version)
    : mPendingRead(0), mPendingWrite(0), mTimeoutTimer(0), mCheckTimer(0), mFinishStatus(0),
      mVersion(version), mSilent(false), mIsConnected(false), mWarned(false)
{
}

Connection::~Connection()
{
    if (std::shared_ptr<EventLoop> eventLoop = EventLoop::eventLoop()) {
        if (mTimeoutTimer)
            eventLoop->unregisterTimer(mTimeoutTimer);
        if (mCheckTimer)
            eventLoop->unregisterTimer(mCheckTimer);
    }
    disconnect();
}

void Connection::disconnect()
{
    if (mSocketClient) {
        mSocketClient->disconnected().disconnect();
        mSocketClient->readyRead().disconnect();
        mSocketClient->bytesWritten().disconnect();
        mSocketClient->error().disconnect();
        mSocketClient.reset();
    }
}

void Connection::connect(const std::shared_ptr<SocketClient> &client)
{
    assert(!mSocketClient);
    mSocketClient = client;
    mIsConnected = true;
    assert(client->isConnected());
    mSocketClient->disconnected().connect(std::bind(&Connection::onClientDisconnected, this, std::placeholders::_1));
    mSocketClient->readyRead().connect(std::bind(&Connection::onDataAvailable, this, std::placeholders::_1, std::placeholders::_2));
    mSocketClient->bytesWritten().connect(std::bind(&Connection::onDataWritten, this, std::placeholders::_1, std::placeholders::_2));
    mSocketClient->error().connect(std::bind(&Connection::onSocketError, this, std::placeholders::_1, std::placeholders::_2));
    mCheckTimer = EventLoop::eventLoop()->registerTimer([this](int) { checkData(); }, 0, Timer::SingleShot);
}

void Connection::checkData()
{
    if (!mSocketClient->buffer().isEmpty())
        onDataAvailable(mSocketClient, std::forward<Buffer>(mSocketClient->takeBuffer()));
}

#ifndef _WIN32
bool Connection::connectUnix(const Path &socketFile, int timeout)
{
    assert(!mSocketClient);
    if (timeout > 0) {
        mTimeoutTimer = EventLoop::eventLoop()->registerTimer([this](int) {
                if (!mIsConnected) {
                    mSocketClient.reset();
                    mDisconnected(shared_from_this());
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
#endif

bool Connection::connectTcp(const String &host, uint16_t port, int timeout)
{
    assert(!mSocketClient);
    if (timeout > 0) {
        mTimeoutTimer = EventLoop::eventLoop()->registerTimer([this](int) {
                if (!mIsConnected) {
                    mSocketClient.reset();
                    mDisconnected(shared_from_this());
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

void Connection::onDataAvailable(const std::shared_ptr<SocketClient> &client, Buffer&& buf)
{
    auto that = shared_from_this();
    while (true) {
        if (!buf.isEmpty())
            mBuffers.push(std::forward<Buffer>(buf));

        unsigned int available = mBuffers.size();
        if (!available)
            break;
        if (!mPendingRead) {
            if (available < static_cast<int>(sizeof(uint32_t)))
                break;
            union {
                unsigned char b[sizeof(uint32_t)];
                int pending;
            };
            const int read = mBuffers.read(b, 4);
            assert(read == 4);
            mPendingRead = pending;
            assert(mPendingRead > 0);
            available -= read;
        }
        assert(mPendingRead >= 0);
        if (available < static_cast<unsigned int>(mPendingRead))
            break;

        StackBuffer<1024 * 16> buffer(mPendingRead);
        const int read = mBuffers.read(buffer.buffer(), mPendingRead);
        assert(read == mPendingRead);
        mPendingRead = 0;
        Message::MessageError error;
        std::shared_ptr<Message> message = Message::create(mVersion, buffer, read, &error);
        if (message) {
            if (message->messageId() == FinishMessage::MessageId) {
                mFinishStatus = std::static_pointer_cast<FinishMessage>(message)->status();
                mFinished(that, mFinishStatus);
            } else {
                newMessage()(message, that);
            }
        } else if (mErrorHandler) {
            mErrorHandler(client, std::move(error));
        } else {
            ::error() << "Unable to create message from data" << error.type << error.text << read;
        }
        if (!message)
            client->close();
    }
}

void Connection::onDataWritten(const std::shared_ptr<SocketClient>&, int bytes)
{
    assert(mPendingWrite >= bytes);
    mPendingWrite -= bytes;
    // ::error() << "wrote some bytes" << mPendingWrite << bytes;
    if (!mPendingWrite) {
        mSendFinished(shared_from_this());
    }
}

class SocketClientBuffer : public Serializer::Buffer
{
public:
    SocketClientBuffer(const std::shared_ptr<SocketClient> &client)
        : mClient(client), mWritten(0)
    {}

    virtual bool write(const void *data, int len) override
    {
        if (mClient->write(data, len)) {
            mWritten += len;
            return true;
        }
        return false;
    }

    virtual int pos() const override
    {
        return mWritten;
    }
private:
    std::shared_ptr<SocketClient> mClient;
    int mWritten;
};

bool Connection::send(const Message &message)
{
    // ::error() << getpid() << "sending message" << static_cast<int>(message.messageId());
    if (!mSocketClient || !mSocketClient->isConnected()) {
        if (!mWarned) {
            mWarned = true;
            warning("Trying to send message to unconnected client (%d)", message.messageId());
        }
        return false;
    }

    mAboutToSend(shared_from_this(), &message);

#ifdef RCT_SERIALIZER_VERIFY_PRIMITIVE_SIZE
    const size_t size = String::npos;
#else
    const size_t size = message.encodedSize();
#endif

    if (size == String::npos || message.mFlags & Message::MessageCache) {
        String header, value;
        message.prepare(mVersion, header, value);
        mPendingWrite += header.size() + value.size();
        assert(size == String::npos || size == (header.size() + value.size() - 4));
        return (mSocketClient->write(header) && (value.isEmpty() || mSocketClient->write(value)));
    } else {
        mPendingWrite += (size + Message::HeaderExtra) + sizeof(int);
        Serializer serializer(std::unique_ptr<SocketClientBuffer>(new SocketClientBuffer(mSocketClient)));
        message.encodeHeader(serializer, size, mVersion);
        message.encode(serializer);
        return !serializer.hasError();
    }
}
