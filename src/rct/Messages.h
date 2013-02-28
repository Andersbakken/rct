#ifndef MESSAGES_H
#define MESSAGES_H

#include <rct/Message.h>
#include <rct/Map.h>
#include <rct/Mutex.h>
#include <rct/MutexLocker.h>

class Message;
class Messages
{
public:
    static Message *create(const char *data, int size);
    template<typename T> static void registerMessage()
    {
        const int id = T::MessageId;
        MutexLocker lock(&sMutex);
        MessageCreatorBase *&base = sFactory[id];
        if (!base)
            base = new MessageCreator<T>();
    }
    static void cleanup();
private:
    class MessageCreatorBase
    {
    public:
        virtual ~MessageCreatorBase() {}
        virtual Message *create(const char *data, int size) = 0;
    };

    template <typename T>
    class MessageCreator : public MessageCreatorBase
    {
    public:
        virtual Message *create(const char *data, int size)
        {
            T *t = new T;
            t->fromData(data, size);
            return t;
        }
    };

    static Map<int, MessageCreatorBase *> sFactory;
    static Mutex sMutex;
};

#endif // MESSAGES_H
