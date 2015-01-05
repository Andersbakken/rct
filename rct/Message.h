#ifndef MESSAGE_H
#define MESSAGE_H

#include <rct/Serializer.h>
#include <mutex>
class Message
{
public:
    enum { ResponseId = 1, FinishMessageId = 2, ConnectMessageId = 3 };

    Message(uint8_t id, uint8_t flags = None) : mMessageId(id), mFlags(flags), mVersion(0) {}
    virtual ~Message() {}

    enum Flag {
        None = 0x0,
        Compressed = 0x1
    };

    uint8_t flags() const { return mFlags; }
    uint8_t messageId() const { return mMessageId; }

    virtual void encode(Serializer &/* serializer */) const {}
    virtual void decode(Deserializer &/* deserializer */) {}

    static std::shared_ptr<Message> create(int version, const char *data, int size);
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

    void prepare(int version, String &header, String &value) const;
    friend class Connection;

    uint8_t mMessageId;
    uint8_t mFlags;
    mutable int mVersion;
    mutable String mHeader;
    mutable String mValue;

    static Map<uint8_t, MessageCreatorBase *> sFactory;
    static std::mutex sMutex;

};

#endif // MESSAGE_H
