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
    case Type_Custom:
        customPtr()->~shared_ptr<Custom>();
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
    case Type_Custom:
        new (mData.customBuf) std::shared_ptr<Custom>(*other.customPtr());
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
        return Value(String(object->valuestring));
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

Value Value::fromJSON(const char *json, bool *ok)
{
    cJSON *obj = cJSON_Parse(json);
    if (!obj) {
        if (ok)
            *ok = false;
        return Value();
    }

    const Value ret = ::fromCJSON(obj);
    if (ok)
        *ok = true;
    cJSON_Delete(obj);
    return ret;
}

cJSON *Value::toCJSON(const Value &value)
{
    switch (value.type()) {
    case Value::Type_Boolean: return value.toBool() ? cJSON_CreateTrue() : cJSON_CreateFalse();
    case Value::Type_Integer: return cJSON_CreateNumber(value.toInteger());
    case Value::Type_Double: return cJSON_CreateNumber(value.toDouble());
    case Value::Type_String: return cJSON_CreateString(value.toString().constData());
    case Value::Type_List: {
        cJSON *array = cJSON_CreateArray();
        for (const auto &v : *value.listPtr())
            cJSON_AddItemToArray(array, toCJSON(v));
        return array; }
    case Value::Type_Map: {
        cJSON *object = cJSON_CreateObject();
        for (const auto &v : *value.mapPtr())
            cJSON_AddItemToObject(object, v.first.constData(), v.second.toCJSON(v.second));
        return object; }
    case Value::Type_Invalid:
        break;
    case Value::Type_Undefined:
        break;
    case Value::Type_Custom:
        if (std::shared_ptr<Value::Custom> custom = value.toCustom()) {
            cJSON *ret = cJSON_CreateString(custom->toString().constData());
            if (ret) {
                ret->type = cJSON_RawString;
                return ret;
            }
        }
        break;
    }
    return cJSON_CreateNull();
}

String Value::toJSON(bool pretty) const
{
    cJSON *json = toCJSON(*this);
    char *formatted = (pretty ? cJSON_Print(json) : cJSON_PrintUnformatted(json));
    cJSON_Delete(json);
    const String ret = formatted;
    free(formatted);
    return ret;
}
