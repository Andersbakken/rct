#ifndef FinishMessage_h
#define FinishMessage_h

#include <rct/Message.h>

class FinishMessage : public Message
{
public:
    enum { MessageId = FinishMessageId };

    FinishMessage(int status = 0)
        : Message(MessageId), mStatus(status)
    {
    }

    int status() const { return mStatus; }

    virtual size_t encodedSize() const override { return sizeof(mStatus); }
    virtual void encode(Serializer &s) const override { s << mStatus; }
    virtual void decode(Deserializer &d) override { d >> mStatus; }
private:
    int mStatus;
};

#endif
