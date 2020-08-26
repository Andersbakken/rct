#include "MessageQueue.h"

#include <string.h>
#include <map>
#include <utility>

#include "rct/Buffer.h"
#include "rct/SignalSlot.h"

#ifdef _WIN32
// todo: implement on windows
#else

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <mutex>

#include "EventLoop.h"
#include "Log.h"
#include "Path.h"
#include "Thread.h"

class MessageThread : public Thread, public std::enable_shared_from_this<MessageThread>
{
public:
    MessageThread(int q, MessageQueue* mq)
        : queueId(q), queue(mq), stopped(false), loop(EventLoop::eventLoop())
    {
    }

    void stop();

protected:
    virtual void run() override;

private:
    static void notifyDataAvailable(const Buffer& buf, const std::weak_ptr<MessageThread>& thread);

private:
    int queueId;
    MessageQueue* queue;
    std::mutex mutex;
    bool stopped;
    std::weak_ptr<EventLoop> loop;
};

void MessageThread::stop()
{
    // Ugly!
    std::unique_lock<std::mutex> locker(mutex);
    stopped = true;
    kill(getpid(), SIGUSR2);
}

void MessageThread::run()
{
    std::weak_ptr<MessageThread> thread = shared_from_this();
    assert(queueId != -1);
    struct {
        long mtype;
        char mtext[4096];
    } msgbuf;

    ssize_t sz;
    for (;;) {
        {
            std::unique_lock<std::mutex> locker(mutex);
            if (stopped)
                break;
        }
        sz = msgrcv(queueId, &msgbuf, sizeof(msgbuf.mtext), 0, 0);
        if (sz == -1) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EIDRM) {
                queueId = -1;
                return;
            } else if (errno == E2BIG) {
                error() << "Data too big in MessageQueue";
            }
        } else {
            assert(sz > 0);
            Buffer buf;
            buf.resize(sz);
            memcpy(buf.data(), msgbuf.mtext, sz);

            if (std::shared_ptr<EventLoop> l = loop.lock()) {
                std::weak_ptr<MessageThread> thr = thread;
                l->callLaterMove(std::bind(MessageThread::notifyDataAvailable, std::placeholders::_1, std::placeholders::_2), std::move(buf), std::move(thr));
            }
        }
    }
}

void MessageThread::notifyDataAvailable(const Buffer& buf, const std::weak_ptr<MessageThread>& thread)
{
    if (std::shared_ptr<MessageThread> thr = thread.lock()) {
        thr->queue->signalDataAvailable(buf);
    }
}

#define PROJID 3947

static pthread_once_t msgInitOnce = PTHREAD_ONCE_INIT;

static void msgSigHandler(int)
{
    // do nothing
}

static void msgInit()
{
    signal(SIGUSR2, msgSigHandler);
}

MessageQueue::MessageQueue(int key, CreateFlag flag)
{
    pthread_once(&msgInitOnce, msgInit);
    const int flg = (flag == Create) ? (IPC_CREAT | IPC_EXCL) : 0;
    queue = msgget(key, flg);
    owner = ((flg & IPC_CREAT) == IPC_CREAT);
    thread = std::make_shared<MessageThread>(queue, this);
    thread->start();
}

MessageQueue::MessageQueue(const Path& path, CreateFlag flag)
    : queue(-1), owner(false)
{
    pthread_once(&msgInitOnce, msgInit);
    const key_t key = ftok(path.nullTerminated(), PROJID);
    if (key == -1)
        return;
    const int flg = (flag == Create) ? (IPC_CREAT | IPC_EXCL) : 0;
    queue = msgget(key, flg);
    owner = ((flg & IPC_CREAT) == IPC_CREAT);
    thread = std::make_shared<MessageThread>(queue, this);
    thread->start();
}

MessageQueue::~MessageQueue()
{
    if (thread) {
        thread->stop();
        thread->join();
        thread.reset();
    }
    if (queue != -1 && owner) {
        msgctl(queue, IPC_RMID, nullptr);
    }
}

bool MessageQueue::send(const char* data, size_t size)
{
    if (queue == -1)
        return false;
    struct {
        long mtype;
        const char* data;
    } msgbuf = { 1, data };
    int ret;
    for (;;) {
        ret = msgsnd(queue, &msgbuf, size, 0);
        if (ret == -1) {
            if (errno == EINTR)
                continue;
            if (errno == EIDRM) {
                if (thread) {
                    thread->stop();
                    thread->join();
                    thread.reset();
                }
                queue = -1;
            }
            return false;
        }
        break;
    }
    return true;
}

#endif
