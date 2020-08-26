#ifndef MESSAGEQUEUE_H
#define MESSAGEQUEUE_H

#include <rct/Buffer.h>
#include <rct/SignalSlot.h>
#include <rct/String.h>
#include <stddef.h>
#include <memory>
#include <functional>

#include "rct/String.h"

class MessageThread;
class Path;

class MessageQueue
{
public:
    enum CreateFlag { None, Create };

    MessageQueue(int key, CreateFlag = None);
    MessageQueue(const Path& path, CreateFlag flag = None);
    ~MessageQueue();

    Signal<std::function<void(const Buffer&)> >& dataAvailable() { return signalDataAvailable; }

    bool send(const String& data) { return send(data.nullTerminated(), data.size()); }
    bool send(const Buffer& data) { return send(reinterpret_cast<const char*>(data.data()), data.size()); }
    bool send(const char* data, size_t size);

private:
    int queue;
    bool owner;
    Signal<std::function<void(const Buffer&)> > signalDataAvailable;
    std::shared_ptr<MessageThread> thread;

    friend class MessageThread;
};

#endif
