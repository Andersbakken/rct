#include "Value.h"
#include "../cJSON/cJSON.h"

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

static Value fromCJSON(const cJSON *object)
{
    assert(object);
    switch (object->type) {
    case cJSON_False:
        return Value(false);
    case cJSON_True:
        return Value(true);
    case cJSON_NULL:
        break;
    case cJSON_Number:
        if (object->valueint == object->valuedouble)
            return Value(object->valueint);
        return Value(object->valuedouble);
    case cJSON_String:
        return Value(object->valuestring);
    case cJSON_Array: {
        List<Value> values;
        for (const cJSON *child = object->child; child; child = child->next) {
            values.append(fromCJSON(child));
        }
        return values; }
    case cJSON_Object: {
        Map<String, Value> values;
        for (const cJSON *child = object->child; child; child = child->next)
            values[child->string] = fromCJSON(child);
        return values; }
    }
    return Value();
}

Value Value::fromJSON(const String &json, bool *ok)
{
    cJSON *obj = cJSON_Parse(json.constData());
    if (!obj) {
        if (!ok)
            *ok = false;
        return Value();
    }

    const Value ret = ::fromCJSON(obj);
    if (ok)
        *ok = true;
    cJSON_Delete(obj);
    return ret;
}
