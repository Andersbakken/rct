#ifndef FinishMessage_h
#define FinishMessage_h

#include <rct/Message.h>
#include <rct/String.h>

class FinishMessage : public Message
{
public:
    enum { MessageId = FinishMessageId };

    FinishMessage()
        : Message(MessageId)
    {
    }

    void encode(Serializer &) const {}
    void decode(Deserializer &) {}
};

#endif
