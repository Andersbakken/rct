#ifndef FinishMessage_h
#define FinishMessage_h

#include <rct/Message.h>
#include <rct/String.h>

class FinishMessage : public Message
{
public:
    enum { MessageId = FinishMessageId };

    FinishMessage(int status = 0)
        : Message(MessageId), mStatus(status)
    {
    }

    int status() const { return mStatus; }

    void encode(Serializer &s) const { s << mStatus; }
    void decode(Deserializer &d) { d >> mStatus; }
private:
    int mStatus;
};

#endif
