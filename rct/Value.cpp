#include "Value.h"

void Value::clear()
{
    switch (mType) {
    case Type_String:
        stringPtr()->~String();
        break;
    case Type_Map:
        mapPtr()->~Map<String, Value>();
        break;
    case Type_List:
        listPtr()->~List<Value>();
        break;
    default:
        break;
    }

    mType = Type_Invalid;
}

void Value::copy(const Value &other)
{
    assert(isNull());
    mType = other.mType;
    switch (mType) {
    case Type_String:
        new (mData.stringBuf) String(*other.stringPtr());
        break;
    case Type_Map:
        new (mData.mapBuf) Map<String, Value>(*other.mapPtr());
        break;
    case Type_List:
        new (mData.listBuf) List<Value>(*other.listPtr());
        break;
    default:
        memcpy(&mData, &other.mData, sizeof(mData));
        break;
    }
}
