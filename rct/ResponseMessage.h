#ifndef ResponseMessage_h
#define ResponseMessage_h

#include <rct/Message.h>
#include <rct/String.h>

class ResponseMessage : public Message
{
public:
    enum { MessageId = ResponseId };
    enum Type { Stdout, Stderr };

    ResponseMessage(const String &data = String(), Type type = Stdout)
        : Message(MessageId), mData(data), mType(type)
    {
        if (mData.endsWith('\n'))
            mData.chop(1);
    }
    ResponseMessage(const List<String> &data, Type type = Stdout)
        : Message(MessageId), mData(String::join(data, "\n")), mType(type)
    {
        if (mData.endsWith('\n'))
            mData.chop(1);
    }

    Type type() const { return mType; }

    String data() const { return mData; }
    void setData(const String &data) { mData = data; }

    virtual size_t encodedSize() const override { return mData.size() + sizeof(int); }
    virtual void encode(Serializer &serializer) const override { serializer << mData; }
    virtual void decode(Deserializer &deserializer) override { deserializer >> mData; }
private:
    String mData;
    Type mType;
};

#endif
