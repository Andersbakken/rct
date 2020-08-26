#include "Message.h"

#include <stdint.h>
#include <cstdlib>
#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <utility>

#include "FinishMessage.h"
#include "QuitMessage.h"
#include "ResponseMessage.h"
#include "Serializer.h"
#include "rct/Log.h"
#include "rct/Map.h"
#include "rct/Message.h"
#include "rct/String.h"

std::mutex Message::sMutex;
Map<uint8_t, Message::MessageCreatorBase *> Message::sFactory;

void Message::prepare(int version, String &header, String &value) const
{
    if (mHeader.isEmpty() || version != mVersion) {
        if (version != mVersion) {
            mHeader.clear();
            mValue.clear();
        }
        {
            Serializer s(mValue);
            encode(s);
        }
        if (mFlags & Compressed) {
            mValue = mValue.compress();
        }
        Serializer s(mHeader);
        encodeHeader(s, mValue.size(), version);
        mVersion = version;
    }
    value = mValue;
    header = mHeader;
}

std::shared_ptr<Message> Message::create(int version, const char *data, int size, MessageError *errorPtr)
{
    auto sendError = [errorPtr](MessageErrorType type, const String &text) {
        if (errorPtr) {
            errorPtr->text = text;
            errorPtr->type = type;
        } else {
            logDirect(LogLevel::Error, text);
        }
    };
    if (!size || !data) {
        sendError(Message_LengthError, "Can't create message from empty data");
        return std::shared_ptr<Message>();
    }
    Deserializer ds(data, Serializer::sizeOf<int>() + Serializer::sizeOf<uint8_t>() + Serializer::sizeOf<uint8_t>());
    int ver;
    ds >> ver;
    if (ver != version) {
        String text;
        size -= Serializer::sizeOf(ver);
        if (size > 1) {
            uint8_t id;
            ds >> id;
            text = String::format<1024>("Invalid message version. Got %d, expected %d id: %d", ver, version, id);
            text += String::toHex(data, std::min(size, 1024));
        } else {
            text = String::format<1024>("Invalid message version. Got %d, expected %d", ver, version);
            text += String::toHex(data, std::min(size, 1024));
        }
        sendError(Message_VersionError, text);
        return std::shared_ptr<Message>();
    }
    size -= Serializer::sizeOf(version);
    data += Serializer::sizeOf(version);
    uint8_t id;
    ds >> id;
    size -= Serializer::sizeOf(id);
    data += Serializer::sizeOf(id);
    uint8_t flags;
    ds >> flags;
    data += Serializer::sizeOf(flags);
    size -= Serializer::sizeOf(flags);
    String uncompressed;
    if (flags & Compressed) {
        uncompressed = String::uncompress(data, size);
        data = uncompressed.constData();
        size = uncompressed.size();
    }
    std::lock_guard<std::mutex> lock(sMutex);
    if (!sFactory.contains(ResponseMessage::MessageId)) {
        atexit(Message::cleanup);
        sFactory[ResponseMessage::MessageId] = new MessageCreator<ResponseMessage>();
        sFactory[FinishMessage::MessageId] = new MessageCreator<FinishMessage>();
        sFactory[QuitMessage::MessageId] = new MessageCreator<QuitMessage>();
    }

    MessageCreatorBase *base = sFactory.value(id);
    if (!base) {
        sendError(Message_IdError, String::format<128>("Invalid message id %d, data: %d bytes", id, size));
        return std::shared_ptr<Message>();
    }
    std::shared_ptr<Message> message(base->create(data, size));
    if (!message) {
        sendError(Message_CreateError, String::format<128>("Can't create message from data id: %d, data: %d bytes", id, size));
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
