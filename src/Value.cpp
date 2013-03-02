#include "rct/Value.h"

void Value::clear()
{
    if (mType == Type_String)
        stringPtr()->~String();
    mType = Type_Invalid;
}

void Value::copy(const Value &other)
{
    mType = other.mType;
    if (mType == Type_String) {
        new (mData.stringBuf) String(*other.stringPtr());
    } else {
        memcpy(&mData, &other.mData, sizeof(mData));
    }
}
