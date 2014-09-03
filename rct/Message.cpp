#include "Message.h"
#include "Messages.h"

void Message::prepare(String &header, String &value) const
{
    if (mHeader.isEmpty()) {
        {
            Serializer s(mValue);
            encode(s);
        }
        const bool z = compress(mValue);
        if (z) {
            mValue = mValue.compress();
        }
        Serializer s(mHeader);
        s << static_cast<uint32_t>(sizeof(uint8_t) + sizeof(uint8_t) + sizeof(bool) + value.size())
          << static_cast<int8_t>(Messages::Version) << z << mMessageId;
    }
    value = mValue;
    header = mHeader;
}
