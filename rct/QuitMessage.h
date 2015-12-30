#ifndef QuitMessage_h
#define QuitMessage_h

#include "Message.h"

class QuitMessage : public Message
{
public:
    enum { MessageId = QuitMessageId };

    QuitMessage(int exitCode = 0)
        : Message(MessageId), mExitCode(exitCode)
    {
    }

    int exitCode() const { return mExitCode; }
    virtual size_t encodedSize() const override { return sizeof(int); }
    void encode(Serializer &s) const override { s << mExitCode; }
    void decode(Deserializer &s) override { s >> mExitCode; }
private:
    int mExitCode;
};

#endif
