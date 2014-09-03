#ifndef MESSAGE_H
#define MESSAGE_H

#include <rct/Serializer.h>
class Message
{
public:
    enum { ResponseId = 1, FinishMessageId = 2, ConnectMessageId = 3 };

    Message(uint8_t id) : mMessageId(id) {}
    virtual ~Message() {}

    uint8_t messageId() const { return mMessageId; }

    virtual void encode(Serializer &/* serializer */) const {}
    virtual void decode(Deserializer &/* deserializer */) {}

    virtual bool compress(const String &value) const { return true; }
private:
    void prepare(String &header, String &value) const;
private:
    friend class Connection;
    uint8_t mMessageId;
    mutable String mHeader;
    mutable String mValue;
};

#endif // MESSAGE_H
