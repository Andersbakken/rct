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

    virtual int encodedSize() const { return 0; }
    void encode(Serializer &) const {}
    void decode(Deserializer &) {}
};

#endif
