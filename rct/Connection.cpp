#include "rct/Connection.h"
#include "rct/SocketClient.h"
#include "rct/EventLoop.h"
#include "rct/Serializer.h"
#include "rct/Messages.h"
#include "rct/Timer.h"
#include <assert.h>

#include "Connection.h"

Connection::Connection()
    : mSocketClient(new SocketClient(SocketClient::Unix)), mPendingRead(0), mPendingWrite(0), mDone(false), mSilent(false), mFinished(false)
{
    mSocketClient->connected().connect(std::bind(&Connection::onClientConnected, this, std::placeholders::_1));
    mSocketClient->disconnected().connect(std::bind(&Connection::onClientDisconnected, this, std::placeholders::_1));
    mSocketClient->readyRead().connect(std::bind(&Connection::onDataAvailable, this, std::placeholders::_1));
    mSocketClient->bytesWritten().connect(std::bind(&Connection::onDataWritten, this, std::placeholders::_1, std::placeholders::_2));
    mSocketClient->error().connect(std::bind(&Connection::onSocketError, this, std::placeholders::_1, std::placeholders::_2));
}

Connection::Connection(const SocketClient::SharedPtr &client)
    : mSocketClient(client), mPendingRead(0), mPendingWrite(0), mDone(false), mSilent(false), mFinished(false)
{
    assert(client->isConnected());
    mSocketClient->disconnected().connect(std::bind(&Connection::onClientDisconnected, this, std::placeholders::_1));
    mSocketClient->readyRead().connect(std::bind(&Connection::onDataAvailable, this, std::placeholders::_1));
    mSocketClient->bytesWritten().connect(std::bind(&Connection::onDataWritten, this, std::placeholders::_1, std::placeholders::_2));
    mSocketClient->error().connect(std::bind(&Connection::onSocketError, this, std::placeholders::_1, std::placeholders::_2));
    //EventLoop::eventLoop()->callLater(std::bind(&Connection::checkData, this));
    mCheckDataTimer =
      EventLoop::eventLoop()->
      registerTimer([&](int){ this->checkData(); }, 1, Timer::SingleShot);
}

void Connection::checkData()
{
    if (!mSocketClient->buffer().isEmpty())
        onDataAvailable(mSocketClient);
}

bool Connection::connectToServer(const String &name, int timeout)
{
    // ### need to revisit this
    // if (timeout != -1)
    //     EventLoop::eventLoop()->registerTimer([=](int) {
    //             if (mClient->state() == SocketClient::Connecting)
    //                 mClient->close();
    //         }, timeout, Timer::SingleShot);
    return mSocketClient->connect(name);
}

bool Connection::send(int id, const String &message)
{
    if (message.isEmpty())
        return true;

    if (!mSocketClient->isConnected()) {
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
    return mSocketClient->write(header) && mSocketClient->write(data);
}

int Connection::pendingWrite() const
{
    return mPendingWrite;
}

void Connection::finish()
{
    if (mFinished)
        return;
    mFinished = true;
    mDone = true;
    EventLoop::eventLoop()->unregisterTimer( mCheckDataTimer );
    onDataWritten(mSocketClient, 0);
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

void Connection::onDataAvailable(SocketClient::SharedPtr&)
{
    while (true) {
        if (!mSocketClient->buffer().isEmpty())
            mBuffers.push_back(std::move(mSocketClient->takeBuffer()));
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
            available -= 4;
        }
        if (available < mPendingRead)
            break;
        char buf[1024];
        char *buffer = buf;
        if (mPendingRead > static_cast<int>(sizeof(buf))) {
            buffer = new char[mPendingRead];
        }
        const int read = bufferRead(mBuffers, buffer, mPendingRead);
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

void Connection::onDataWritten(const SocketClient::SharedPtr&, int bytes)
{
    assert(mPendingWrite >= bytes);
    mPendingWrite -= bytes;
    if (!mPendingWrite) {
        if (bytes)
            mSendComplete(this);
        if (mDone)
            mSocketClient->close();
    }
}

void Connection::writeAsync(const String &out)
{
    EventLoop::eventLoop()->callLaterMove(std::bind(&Connection::sendRef, this, std::placeholders::_1), ResponseMessage(out));
}
