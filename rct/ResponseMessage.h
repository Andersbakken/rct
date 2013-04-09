#ifndef ResponseMessage_h
#define ResponseMessage_h

#include <rct/Message.h>
#include <rct/String.h>

class ResponseMessage : public Message
{
public:
    enum { MessageId = ResponseId };

    ResponseMessage(const String &data = String())
        : Message(MessageId), mData(data)
    {
        if (mData.endsWith('\n'))
            mData.chop(1);
    }
    ResponseMessage(const List<String> &data)
        : Message(MessageId), mData(String::join(data, "\n"))
    {
        if (mData.endsWith('\n'))
            mData.chop(1);
    }

    String data() const { return mData; }
    void setData(const String &data) { mData = data; }

    void encode(Serializer &serializer) const { serializer << mData; }
    void decode(Deserializer &deserializer) { deserializer >> mData; }
private:
    String mData;
};

#endif
