#ifndef MESSAGE_H
#define MESSAGE_H

#include <rct/Serializer.h>
class Message
{
public:
    enum { ResponseId = 1 };

    Message(uint8_t id) : mMessageId(id) {}
    virtual ~Message() {}

    uint8_t messageId() const { return mMessageId; }

    virtual void encode(Serializer &serializer) const = 0;
    virtual void decode(Deserializer &deserializer) = 0;
private:
    uint8_t mMessageId;
};

#endif // MESSAGE_H
