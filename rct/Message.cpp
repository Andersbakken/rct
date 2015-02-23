#include "Message.h"
#include "ResponseMessage.h"
#include "FinishMessage.h"
#include "ConnectMessage.h"
#include "Serializer.h"
#include <assert.h>

std::mutex Message::sMutex;
Map<uint8_t, Message::MessageCreatorBase *> Message::sFactory;

void Message::prepare(int version, String &header, String &value) const
{
    if (mHeader.isEmpty() || version != mVersion) {
        {
            Serializer s(mValue);
            encode(s);
        }
        if (mFlags & Compressed) {
            mValue = mValue.compress();
        }
        Serializer s(mHeader);
        s << static_cast<uint32_t>(sizeof(int) + sizeof(uint8_t) + sizeof(uint8_t) + mValue.size())
          << version << static_cast<uint8_t>(mMessageId) << mFlags;
        mVersion = version;
    }
    value = mValue;
    header = mHeader;
}

std::shared_ptr<Message> Message::create(int version, const char *data, int size)
{
    if (!size || !data) {
        error("Can't create message from empty data");
        return std::shared_ptr<Message>();
    }
    Deserializer ds(data, sizeof(int) + sizeof(uint8_t) + sizeof(uint8_t));
    int ver;
    ds >> ver;
    if (ver != version) {
        size -= sizeof(ver);
        if (size > 1) {
            uint8_t id;
            ds >> id;
            error("Invalid message version. Got %d, expected %d id: %d", ver, version, id);
            error() << String::toHex(data, std::min(size, 1024));
            // error() << Rct::backtrace();
        } else {
            error("Invalid message version. Got %d, expected %d", ver, version);
            error() << String::toHex(data, std::min(size, 1024));
        }
        return std::shared_ptr<Message>();
    }
    size -= sizeof(version);
    data += sizeof(version);
    uint8_t id;
    ds >> id;
    size -= sizeof(id);
    data += sizeof(id);
    uint8_t flags;
    ds >> flags;
    data += sizeof(flags);
    size -= sizeof(flags);
    String uncompressed;
    if (flags & Compressed) {
        uncompressed = String::uncompress(data, size);
        data = uncompressed.constData();
        size = uncompressed.size();
    }
    std::lock_guard<std::mutex> lock(sMutex);
    if (!sFactory.contains(ResponseMessage::MessageId)) {
        sFactory[ResponseMessage::MessageId] = new MessageCreator<ResponseMessage>();
        sFactory[FinishMessage::MessageId] = new MessageCreator<FinishMessage>();
        sFactory[ConnectMessage::MessageId] = new MessageCreator<ConnectMessage>();
    }

    MessageCreatorBase *base = sFactory.value(id);
    if (!base) {
        error("Invalid message id %d, data: %d bytes, factory %p", id, size, &sFactory);
        return std::shared_ptr<Message>();
    }
    std::shared_ptr<Message> message(base->create(data, size));
    if (!message) {
        error("Can't create message from data id: %d, data: %d bytes", id, size);
    }
    return message;
}

void Message::cleanup()
{
    std::lock_guard<std::mutex> lock(sMutex);
    for (Map<uint8_t, MessageCreatorBase *>::const_iterator it = sFactory.begin(); it != sFactory.end(); ++it)
        delete it->second;
    sFactory.clear();
}

struct Janitor {
    ~Janitor() { Message::cleanup(); }
} janitor;
