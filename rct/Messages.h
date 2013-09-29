#ifndef MESSAGES_H
#define MESSAGES_H

#include <rct/Message.h>
#include <rct/Map.h>
#include <mutex>

class Message;
class Messages
{
public:
    static Message *create(const char *data, int size);
    template<typename T> static void registerMessage()
    {
        const uint8_t id = T::MessageId;
        std::lock_guard<std::mutex> lock(sMutex);
        MessageCreatorBase *&base = sFactory[id];
        if (!base) {
            base = new MessageCreator<T>();
        }
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
            Deserializer deserializer(data, size);
            t->decode(deserializer);
            return t;
        }
    };

    static Map<uint8_t, MessageCreatorBase *> sFactory;
    static std::mutex sMutex;
};

#endif // MESSAGES_H
