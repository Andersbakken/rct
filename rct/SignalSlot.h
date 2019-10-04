#ifndef SIGNALSLOT_H
#define SIGNALSLOT_H

#include <assert.h>
#include <map>
#include <mutex>

#include <rct/EventLoop.h>

template<typename Signature>
class Signal
{
public:
    typedef unsigned int Key;

    Signal() : id(0) { }
    ~Signal() { }

    template<typename Call>
    Key connect(Call&& call)
    {
        std::lock_guard<std::mutex> locker(mutex);
        connections.insert(std::make_pair(++id, std::forward<Call>(call)));
        return id;
    }

    template<size_t Value, typename Call, typename std::enable_if<Value == EventLoop::Async, int>::type = 0>
    Key connect(Call&& call)
    {
        std::lock_guard<std::mutex> locker(mutex);
        connections.insert(std::make_pair(++id, SignatureWrapper(std::forward<Call>(call))));
        return id;
    }

    // this connection type will std::move all the call arguments so if this type is used
    // then no other connections may be used on the same signal
    template<size_t Value, typename Call, typename std::enable_if<Value == EventLoop::Move, int>::type = 0>
    Key connect(Call&& call)
    {
        std::lock_guard<std::mutex> locker(mutex);
        assert(connections.empty());
        connections.insert(std::make_pair(++id, SignatureMoveWrapper(std::forward<Call>(call))));
        return id;
    }

    bool disconnect(Key key)
    {
        std::lock_guard<std::mutex> locker(mutex);
        return connections.erase(key) == 1;
    }

    int disconnect()
    {
        std::lock_guard<std::mutex> locker(mutex);
        const int ret = connections.size();
        connections.clear();
        return ret;
    }

    // ignore result_type for now
    template<typename... Args>
    void operator()(Args&&... args)
    {
        std::map<Key, Signature> conn;
        {
            std::lock_guard<std::mutex> locker(mutex);
            conn = connections;
        }
        for (auto& connection : conn) {
            connection.second(std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    void operator()(const Args&... args)
    {
        std::map<Key, Signature> conn;
        {
            std::lock_guard<std::mutex> locker(mutex);
            conn = connections;
        }
        for (auto& connection : conn) {
            connection.second(std::forward<const Args &>(args)...);
        }
    }

    template<typename... Args>
    void operator()()
    {
        std::map<Key, Signature> conn;
        {
            std::lock_guard<std::mutex> locker(mutex);
            conn = connections;
        }
        for (auto& connection : conn) {
            connection.second();
        }
    }

private:
    class SignatureWrapper
    {
    public:
        SignatureWrapper(Signature&& signature)
            : loop(EventLoop::eventLoop()), call(std::move(signature))
        {
        }

        template<typename... Args>
        void operator()(Args&&... args)
        {
            std::shared_ptr<EventLoop> l;
            if ((l = loop.lock())) {
                l->post(call, std::forward<Args>(args)...);
            }
        }

        std::weak_ptr<EventLoop> loop;
        Signature call;
    };

    class SignatureMoveWrapper
    {
    public:
        SignatureMoveWrapper(Signature&& signature)
            : loop(EventLoop::eventLoop()), call(std::move(signature))
        {
        }

        template<typename... Args>
        void operator()(Args&&... args)
        {
            std::shared_ptr<EventLoop> l;
            if ((l = loop.lock())) {
                l->postMove(call, std::forward<Args>(args)...);
            }
        }

        std::weak_ptr<EventLoop> loop;
        Signature call;
    };

private:
    Key id;
    std::mutex mutex;
    std::map<Key, Signature> connections;
};

#endif
