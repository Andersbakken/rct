#include "Messages.h"
#include "ResponseMessage.h"
#include "FinishMessage.h"
#include "ConnectMessage.h"
#include "Serializer.h"
#include <assert.h>

std::mutex Messages::sMutex;
Map<uint8_t, Messages::MessageCreatorBase *> Messages::sFactory;

Message* Messages::create(const char *data, int size)
{
    if (!size || !data) {
        error("Can't create message from empty data");
        return 0;
    }
    Deserializer ds(data, sizeof(uint8_t) + sizeof(int8_t));
    int8_t version;
    ds >> version;
    if (version != Version) {
        error("Invalid message version. Got %d, expected %d", version, Version);
        return 0;
    }
    size -= sizeof(version);
    data += sizeof(version);
    uint8_t id;
    ds >> id;
    size -= sizeof(id);
    data += sizeof(id);
    std::lock_guard<std::mutex> lock(sMutex);
    if (!sFactory.contains(ResponseMessage::MessageId)) {
        sFactory[ResponseMessage::MessageId] = new MessageCreator<ResponseMessage>();
        sFactory[FinishMessage::MessageId] = new MessageCreator<FinishMessage>();
        sFactory[ConnectMessage::MessageId] = new MessageCreator<ConnectMessage>();
    }

    MessageCreatorBase *base = sFactory.value(id);
    if (!base) {
        error("Invalid message id %d, data: %d bytes, factory %p", id, size, &sFactory);
        return 0;
    }
    Message *message = base->create(data, size);
    if (!message) {
        error("Can't create message from data id: %d, data: %d bytes", id, size);
        return 0;
    }
    return message;
}

void Messages::cleanup()
{
    std::lock_guard<std::mutex> lock(sMutex);
    for (Map<uint8_t, MessageCreatorBase *>::const_iterator it = sFactory.begin(); it != sFactory.end(); ++it)
        delete it->second;
    sFactory.clear();
}

struct Janitor {
    ~Janitor() { Messages::cleanup(); }
} janitor;
