#ifndef ConnectMessage_h
#define ConnectMessage_h

#include <rct/Message.h>
#include <rct/String.h>

class ConnectMessage : public Message
{
public:
    enum { MessageId = ConnectMessageId };

    ConnectMessage()
        : Message(MessageId)
    {
    }

    virtual int encodedSize() const override { return 0; }
    virtual void encode(Serializer &) const override {}
    virtual void decode(Deserializer &) override {}
};

#endif
