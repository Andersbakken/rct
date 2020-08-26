#ifndef MESSAGE_H
#define MESSAGE_H

#include <rct/Serializer.h>
#include <mutex>
#include <memory>

class Message
{
public:
    enum {
        ResponseId = 1,
        FinishMessageId = 2,
        QuitMessageId = 3
    };

    Message(uint8_t id, uint8_t f = None)
        : mMessageId(id), mFlags(f), mVersion(0)
    {}
    virtual ~Message()
    {}

    void clearCache()
    {
        mHeader.clear();
        mValue.clear();
        mVersion = 0;
    }

    enum Flag {
        None = 0x0,
        Compressed = 0x1,
        MessageCache = 0x2
    };

    uint8_t flags() const { return mFlags; }
    uint8_t messageId() const { return mMessageId; }

    virtual void encode(Serializer &/* serializer */) const = 0;
    virtual void decode(Deserializer &/* deserializer */) = 0;

    virtual size_t encodedSize() const { return String::npos; }
    enum MessageErrorType {
        Message_Success,
        Message_VersionError,
        Message_IdError,
        Message_LengthError,
        Message_CreateError
    };
    struct MessageError {
        MessageErrorType type = Message_Success;
        String text;
    };
    static std::shared_ptr<Message> create(int version, const char *data, int size, MessageError *error = nullptr);
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
        virtual Message *create(const char *data, int size) override
        {
            T *t = new T;
            Deserializer deserializer(data, size);
            t->decode(deserializer);
            return t;
        }
    };

    void prepare(int version, String &header, String &value) const;
    enum { HeaderExtra = Serializer::sizeOf<int>() + Serializer::sizeOf<uint8_t>() + Serializer::sizeOf<uint8_t>() };
    inline void encodeHeader(Serializer &serializer, uint32_t size, int version) const
    {
        size += HeaderExtra;
        serializer.write(&size, sizeof(size));
        serializer << version << static_cast<uint8_t>(mMessageId) << mFlags;
    }
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
