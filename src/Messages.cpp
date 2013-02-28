#include "rct/Messages.h"
#include "rct/ResponseMessage.h"
#include "rct/Serializer.h"
#include <assert.h>

Mutex Messages::sMutex;
Map<int, Messages::MessageCreatorBase *> Messages::sFactory;

Message* Messages::create(const char *data, int size)
{
    if (size < static_cast<int>(sizeof(int))) {
        error("Can't create message from data (%d)", size);
        return 0;
    }
    Deserializer ds(data, sizeof(int));
    int id;
    ds >> id;
    size -= sizeof(int);
    data += sizeof(int);
    MutexLocker lock(&sMutex);
    if (!sFactory.contains(ResponseMessage::MessageId))
        sFactory[ResponseMessage::MessageId] = new MessageCreator<ResponseMessage>();

    MessageCreatorBase *base = sFactory.value(id);
    if (!base) {
        error("Can't create message from data id: %d, data: %d bytes", id, size);
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
    MutexLocker lock(&sMutex);
    for (Map<int, MessageCreatorBase *>::const_iterator it = sFactory.begin(); it != sFactory.end(); ++it)
        delete it->second;
    sFactory.clear();
}

struct Janitor {
    ~Janitor() { Messages::cleanup(); }
} janitor;
