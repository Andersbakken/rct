#ifndef MESSAGE_H
#define MESSAGE_H

#include <rct/Serializer.h>
class Message
{
public:
    enum { ResponseId = 1 };

    Message(int id) : mMessageId(id) {}
    virtual ~Message() {}

    int messageId() const { return mMessageId; }

    virtual void encode(Serializer &serializer) const = 0;
    virtual void decode(Deserializer &deserializer) = 0;
private:
    int mMessageId;
};

#endif // MESSAGE_H
