#include "ScriptEngine.h"
#include <v8.h>
#include <rct/EventLoop.h>

struct ScriptEnginePrivate
{
    v8::Persistent<v8::Context> context;
    // v8::Persistent<v8::Function> mParse;
    v8::Isolate *isolate;
};

static String toString(v8::Handle<v8::Value> value);
static v8::Handle<v8::Value> toV8(const Value& value);
static Value fromV8(v8::Handle<v8::Value> value);

ScriptEngine *ScriptEngine::sInstance = 0;
ScriptEngine::ScriptEngine()
    : mPrivate(new ScriptEnginePrivate)
{
    assert(!sInstance);
    sInstance = this;

    mPrivate->isolate = v8::Isolate::New();
    const v8::Isolate::Scope isolateScope(mPrivate->isolate);
    v8::HandleScope handleScope;
    v8::Handle<v8::ObjectTemplate> globalObjectTemplate = v8::ObjectTemplate::New();
    // globalObjectTemplate->Set(v8::String::New("log"), v8::FunctionTemplate::New(log));

#ifdef V8_NEW_CONTEXT_TAKES_ISOLATE
    v8::Handle<v8::Context> ctx = v8::Context::New(mPrivate->isolate, 0, globalObjectTemplate);
    mPrivate->context.Reset(mPrivate->isolate, ctx);
#else
    mPrivate->context = v8::Context::New(0, globalObjectTemplate);
#endif
    // v8::Context::Scope scope(mPrivate->context);
    // assert(!mPrivate->context.IsEmpty());

    // const String esprimaSrcString = Path(ESPRIMA_JS).readAll();
    // v8::Handle<v8::String> esprimaSrc = v8::String::New(esprimaSrcString.constData(), esprimaSrcString.size());

    // v8::TryCatch tryCatch;
    // v8::Handle<v8::Script> script = v8::Script::Compile(esprimaSrc);
    // if (tryCatch.HasCaught() || script.IsEmpty() || !tryCatch.Message().IsEmpty()) {
    //     v8::Handle<v8::Message> message = tryCatch.Message();
    //     v8::String::Utf8Value msg(message->Get());
    //     printf("%s:%d:%d: esprima error: %s {%d-%d}\n", ESPRIMA_JS, message->GetLineNumber(),
    //            message->GetStartColumn(), *msg, message->GetStartPosition(), message->GetEndPosition());
    //     return false;
    // }
    // script->Run();

    // v8::Handle<v8::Object> global = mPrivate->context->Global();
    // mParse = getPersistent<v8::Function>(global, "indexFile");

    // return !mParse.IsEmpty() && mParse->IsFunction();
}

ScriptEngine::~ScriptEngine()
{
    assert(sInstance == this);
    sInstance = 0;
    delete mGlobalObject;
}

bool ScriptEngine::checkSyntax(const String &source, const Path &path, String *error) const
{

}

Value ScriptEngine::evaluate(const String &source, const Path &path, String *error)
{
    const v8::Isolate::Scope isolateScope(mPrivate->isolate);
    v8::HandleScope handleScope;
    v8::Handle<v8::String> src = v8::String::New(source.constData(), source.size());

    v8::TryCatch tryCatch;
    v8::Handle<v8::Script> script = v8::Script::Compile(src);
    if (tryCatch.HasCaught() || script.IsEmpty() || !tryCatch.Message().IsEmpty()) {
        if (error) {
            v8::Handle<v8::Message> message = tryCatch.Message();
            v8::String::Utf8Value msg(message->Get());
            *error = String::format<128>("%s:%d:%d: esprima error: %s {%d-%d}\n", path.constData(), message->GetLineNumber(),
                                         message->GetStartColumn(), *msg, message->GetStartPosition(), message->GetEndPosition());
        }
        return Value();
    }
    script->Run();
}


void ScriptEngine::throwError(const String &error)
{
}

class ObjectPrivate
{
public:
    ObjectPrivate()
    {
    }
    ~ObjectPrivate()
    {
        for (const auto &it : mMembers)
            delete it.second;
    }
    struct Member {
        enum Type {
            Type_Property,
            Type_PropertyReadOnly,
            Type_Function,
            Type_Child
        } type;
        ScriptEngine::Object::Function function;
        struct {
            ScriptEngine::Object::Setter setter;
            ScriptEngine::Object::Getter getter;
        } property;
        ScriptEngine::Object *child;

        Member(ScriptEngine::Object *c)
            : type(Type_Child), child(c)
        {
        }

        Member(ScriptEngine::Object::Getter &&getter)
            : type(Type_PropertyReadOnly), child(0)
        {
            property.getter = std::move(getter);
        }

        Member(ScriptEngine::Object::Getter &&getter, ScriptEngine::Object::Setter &&setter)
            : type(Type_Property), child(0)
        {
            property.getter = std::move(getter);
            property.setter = std::move(setter);
        }

        Member(ScriptEngine::Object::Function &&func)
            : type(Type_Function), function(std::move(func)), child(0)
        {
        }

        ~Member()
        {
            delete child;
        }
    };

    Hash<String, Member*> mMembers;
};

ScriptEngine::Object::Object()
    : mPrivate(new ObjectPrivate)
{
}

ScriptEngine::Object::~Object()
{
    delete mPrivate;
}

// static JSBool jsCallback(JSContext *context, unsigned argc, JS::Value *vp)
// {
//     JS::CallArgs callArgs = JS::CallArgsFromVp(argc, vp);
//     JSObject &callee = callArgs.callee();
//     ObjectPrivate::Member *member = static_cast<ObjectPrivate::Member*>(JS_GetPrivate(&callee));
//     if (!member) {
//         JS_ReportError(context, "Invalid function called");
//         return false;
//     }
//     assert(member);
//     const int length = callArgs.length();
//     List<Value> args(length);
//     for (int i=0; i<length; ++i)
//         args[i] = toRct(context, &callArgs[i]);
//     const Value retVal = member->function(args);
//     callArgs.rval().set(fromRct(context, retVal));
//     return true;
// }

void ScriptEngine::Object::registerFunction(const String &name, Function &&func)
{
    // ObjectPrivate::Member *&member = mPrivate->mMembers[name];
    // assert(!member);
    // JSContext *context = ScriptEngine::instance()->context();
    // JSFunction *jsFunc = JS_DefineFunction(context, mPrivate->mObject, name.constData(), jsCallback, 0, JSPROP_ENUMERATE);
    // if (jsFunc) {
    //     JSObject *obj = JS_GetFunctionObject(jsFunc);
    //     if (obj) {
    //         member = new ObjectPrivate::Member(std::move(func));
    //         JS_SetPrivate(obj, member);
    //     }
    // }
}

void ScriptEngine::Object::registerProperty(const String &name, Getter &&get)
{

}

void ScriptEngine::Object::registerProperty(const String &name, Getter &&get, Setter &&set)
{

}

ScriptEngine::Object *ScriptEngine::Object::child(const String &name)
{
    ObjectPrivate::Member *&data = mPrivate->mMembers[name];
    if (!data) {
        data = new ObjectPrivate::Member(new Object);
    }
    assert(data->type == ObjectPrivate::Member::Type_Child);
    return data->child;
}


static String toString(v8::Handle<v8::Value> value)
{
    if (value->IsString()) {
        v8::String::Utf8Value strValue(value);
        return String(*strValue);
    }
    return String();
}

static Value fromV8(v8::Handle<v8::Value> value)
{
    if (value->IsString()) {
        return toString(value);
    } else if (value->IsArray()) {
        v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(value);
        List<Value> result(array->Length());
        for (size_t i = 0; i < array->Length(); ++i)
            result[i] = fromV8(array->Get(i));
        return result;
    } else if (value->IsObject()) {
        Value result;
        v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
        v8::Local<v8::Array> properties = object->GetOwnPropertyNames();
        for(size_t i = 0; i < properties->Length(); ++i) {
            const v8::Handle<v8::Value> key = properties->Get(i);
            result[toString(key)] = fromV8(object->Get(key));
        }
    } else if (value->IsInt32()) {
        return value->IntegerValue();
    } else if (value->IsNumber()) {
        return value->NumberValue();
    } else if (value->IsBoolean()) {
        return value->BooleanValue();
    } else if (!value->IsNull() && !value->IsUndefined()) {
        error() << "Unknown value type in fromV8";
    }
    return Value();
}

static inline v8::Handle<v8::Value> toV8_helper(const Value &value)
{
    v8::Handle<v8::Value> result;
    switch (value.type()) {
    case Value::Type_String:
        result = v8::String::New(value.toString().constData());
        break;
    case Value::Type_List: {
        const int sz = value.count();
        v8::Handle<v8::Array> array = v8::Array::New(sz);
        auto it = value.listBegin();
        for (int i=0; i<sz; ++i) {
            array->Set(i, toV8_helper(*it));
            ++it;
        }
        result = array;
        break; }
    case Value::Type_Map: {
        v8::Handle<v8::Object> object = v8::Object::New();
        const auto end = value.end();
        for (auto it = value.begin(); it != end; ++it)
            object->Set(v8::String::New(it->first.constData()), toV8_helper(it->second));
        result = object;
        break; }
    case Value::Type_Integer:
        result = v8::Integer::New(value.toInt64());
        break;
    case Value::Type_Double:
        result = v8::Number::New(value.toDouble());
        break;
    case Value::Type_Boolean:
        result = v8::Boolean::New(value.toBool());
        break;
    default:
        result = v8::Undefined();
        break;
    }
    return result;
}

static v8::Handle<v8::Value> toV8(const Value& value)
{
    // ContextHolder holder;
    // return holder.handleScope().Close(convertFromValue_helper(value));
}
