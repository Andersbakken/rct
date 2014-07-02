#include "ScriptEngine.h"

#include <v8.h>
#include <rct/EventLoop.h>

static String toString(v8::Handle<v8::Value> value);
static v8::Handle<v8::Value> toV8(v8::Isolate* isolate, const Value& value);
static Value fromV8(v8::Handle<v8::Value> value);

struct ScriptEnginePrivate
{
    v8::Persistent<v8::Context> context;
    v8::Isolate *isolate;
};

class ObjectPrivate
{
public:
    ObjectPrivate()
        : shouldDelete(true)
    {
    }
    ~ObjectPrivate()
    {
    }

    void init(ScriptEnginePrivate* e, const v8::Handle<v8::Object>& o);

    struct FunctionData
    {
        v8::Persistent<v8::Value> ext;
        ScriptEngine::Object::Function func;
    };
    struct PropertyData
    {
        ScriptEngine::Object::Getter getter;
        ScriptEngine::Object::Setter setter;
    };
    enum { Getter = 0x1, Setter = 0x2 };
    void initProperty(const String& name, PropertyData& data, unsigned int mode);

    ScriptEnginePrivate* engine;
    v8::Persistent<v8::Object> object;
    Hash<String, FunctionData> functions;
    Hash<String, PropertyData> properties;
    Hash<String, std::shared_ptr<ScriptEngine::Object> > children;
    bool shouldDelete;

    // awful
    static ObjectPrivate* objectPrivate(ScriptEngine::Object* obj)
    {
        return obj->mPrivate;
    }
};

struct ObjectData
{
    String name;
    std::weak_ptr<ScriptEngine::Object> weak, parent;
};

static void ObjectWeak(const v8::WeakCallbackData<v8::Object, ObjectPrivate>& data)
{
    if (!data.GetParameter()->shouldDelete)
        return;
    assert(data.GetValue()->GetInternalField(0)->IsExternal());
    v8::Handle<v8::External> ext = v8::Handle<v8::External>::Cast(data.GetValue()->GetInternalField(0));
    ObjectData* objData = static_cast<ObjectData*>(ext->Value());
    if (auto p = objData->parent.lock()) {
        ObjectPrivate* priv = ObjectPrivate::objectPrivate(p.get());
        priv->children.erase(objData->name);
    }
    delete objData;
}

static void StringWeak(const v8::WeakCallbackData<v8::Value, String>& data)
{
    delete data.GetParameter();
}

void ObjectPrivate::init(ScriptEnginePrivate* e, const v8::Handle<v8::Object>& o)
{
    engine = e;
    object.Reset(e->isolate, o);
    object.SetWeak(this, ObjectWeak);
    object.MarkIndependent();
}

static inline std::shared_ptr<ScriptEngine::Object> objectFromHolder(const v8::Local<v8::Object>& holder)
{
    // first see if we're the global object
    {
        ScriptEngine* engine = ScriptEngine::instance();
        auto global = engine->globalObject();
        ObjectPrivate* priv = ObjectPrivate::objectPrivate(global.get());
        if (priv->object == holder) {
            return global;
        }
    }
    // no, see if we can get it via the first internal field
    v8::Handle<v8::Value> val = holder->GetInternalField(0);
    if (val.IsEmpty() || !val->IsExternal()) {
        return std::shared_ptr<ScriptEngine::Object>();
    }
    v8::Handle<v8::External> ext = v8::Handle<v8::External>::Cast(val);
    auto data = static_cast<ObjectData*>(ext->Value());
    return data->weak.lock();
}

static void GetterCallback(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    v8::Isolate* iso = info.GetIsolate();
    v8::HandleScope handleScope(iso);

    std::shared_ptr<ScriptEngine::Object> obj = objectFromHolder(info.Holder());
    if (!obj)
        return;

    ObjectPrivate* priv = ObjectPrivate::objectPrivate(obj.get());
    const String prop = toString(property);
    auto it = priv->properties.find(prop);
    if (it == priv->properties.end())
        return;

    info.GetReturnValue().Set(toV8(iso, it->second.getter()));
}

static void SetterCallback(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
    v8::Isolate* iso = info.GetIsolate();
    v8::HandleScope handleScope(iso);

    std::shared_ptr<ScriptEngine::Object> obj = objectFromHolder(info.Holder());
    if (!obj)
        return;

    ObjectPrivate* priv = ObjectPrivate::objectPrivate(obj.get());
    const String prop = toString(property);
    auto it = priv->properties.find(prop);
    if (it == priv->properties.end())
        return;

    it->second.setter(fromV8(value));
}

void ObjectPrivate::initProperty(const String& name, PropertyData& data, unsigned int mode)
{
    v8::Isolate* iso = engine->isolate;
    const v8::Isolate::Scope isolateScope(iso);
    v8::HandleScope handleScope(iso);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, engine->context);
    v8::Context::Scope contextScope(ctx);
    v8::Local<v8::Object> obj = v8::Local<v8::Object>::New(iso, object);
    assert(mode & Getter);
    if (mode & Setter) {
        obj->SetAccessor(v8::String::NewFromUtf8(iso, name.constData()),
                         GetterCallback,
                         SetterCallback);
    } else {
        obj->SetAccessor(v8::String::NewFromUtf8(iso, name.constData()),
                         GetterCallback,
                         0);
    }
}

ScriptEngine *ScriptEngine::sInstance = 0;
ScriptEngine::ScriptEngine()
    : mPrivate(new ScriptEnginePrivate)
{
    assert(!sInstance);
    sInstance = this;

    mPrivate->isolate = v8::Isolate::New();
    const v8::Isolate::Scope isolateScope(mPrivate->isolate);
    v8::HandleScope handleScope(mPrivate->isolate);
    v8::Handle<v8::ObjectTemplate> globalObjectTemplate = v8::ObjectTemplate::New();

    v8::Handle<v8::Context> ctx = v8::Context::New(mPrivate->isolate, 0, globalObjectTemplate);
    // bind the "global" object to itself
    {
        v8::Context::Scope contextScope(ctx);
        v8::Local<v8::Object> global = ctx->Global();
        global->Set(v8::String::NewFromUtf8(mPrivate->isolate, "global"), global);

        mGlobalObject.reset(new Object);
        mGlobalObject->mPrivate->init(mPrivate, global);
        mGlobalObject->mPrivate->shouldDelete = false;
    }
    mPrivate->context.Reset(mPrivate->isolate, ctx);
}

ScriptEngine::~ScriptEngine()
{
    assert(sInstance == this);
    sInstance = 0;
}

static inline bool catchError(v8::TryCatch &tryCatch, const char *header, String *error)
{
    if (!tryCatch.HasCaught())
        return false;

    if (error) {
        v8::Handle<v8::Message> message = tryCatch.Message();
        v8::String::Utf8Value msg(message->Get());
        v8::String::Utf8Value script(message->GetScriptResourceName());
        *error = String::format<128>("%s:%d:%d: %s: %s {%d-%d}", *script, message->GetLineNumber(),
                                     message->GetStartColumn(), header, *msg, message->GetStartPosition(),
                                     message->GetEndPosition());
    }
    return true;
}

static inline v8::Handle<v8::Value> findFunction(v8::Isolate* isolate, const v8::Local<v8::Context>& ctx,
                                                 const String& function, v8::Handle<v8::Value>* that)
{
    // find the function object
    v8::Handle<v8::Value> val = ctx->Global();
    List<String> list = function.split('.');
    for (const String& f : list) {
        if (val.IsEmpty() || !val->IsObject())
            return v8::Handle<v8::Value>();
        if (that)
            *that = val;
        val = v8::Handle<v8::Object>::Cast(val)->Get(v8::String::NewFromUtf8(isolate, f.constData()));
    }
    return val;
}

Value ScriptEngine::call(const String &function, String* error)
{
    const v8::Isolate::Scope isolateScope(mPrivate->isolate);
    v8::HandleScope handleScope(mPrivate->isolate);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(mPrivate->isolate, mPrivate->context);
    v8::Context::Scope contextScope(ctx);

    // find the function object
    v8::Handle<v8::Value> that, val;
    val = findFunction(mPrivate->isolate, ctx, function, &that);
    if (val.IsEmpty() || !val->IsFunction())
        return Value();
    assert(!that.IsEmpty() && that->IsObject());
    v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(val);

    v8::TryCatch tryCatch;
    val = func->Call(that, 0, 0);
    if (catchError(tryCatch, "Call error", error))
        return Value();
    return fromV8(val);
}

Value ScriptEngine::call(const String &function, std::initializer_list<Value> arguments, String* error)
{
    const v8::Isolate::Scope isolateScope(mPrivate->isolate);
    v8::HandleScope handleScope(mPrivate->isolate);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(mPrivate->isolate, mPrivate->context);
    v8::Context::Scope contextScope(ctx);

    // find the function object
    v8::Handle<v8::Value> that, val;
    val = findFunction(mPrivate->isolate, ctx, function, &that);
    if (val.IsEmpty() || !val->IsFunction())
        return Value();
    assert(!that.IsEmpty() && that->IsObject());
    v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(val);

    const auto sz = arguments.size();
    List<v8::Handle<v8::Value> > v8args;
    v8args.reserve(sz);
    const Value* arg = arguments.begin();
    const Value* end = arguments.end();
    while (arg != end) {
        v8args.append(toV8(mPrivate->isolate, *arg));
        ++arg;
    }

    v8::TryCatch tryCatch;
    val = func->Call(that, sz, &v8args.first());
    if (catchError(tryCatch, "Call error", error))
        return Value();
    return fromV8(val);
}

Value ScriptEngine::evaluate(const String &source, const Path &path, String *error)
{
    const v8::Isolate::Scope isolateScope(mPrivate->isolate);
    v8::HandleScope handleScope(mPrivate->isolate);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(mPrivate->isolate, mPrivate->context);
    v8::Context::Scope contextScope(ctx);
    v8::Handle<v8::String> src = v8::String::NewFromUtf8(mPrivate->isolate, source.constData(), v8::String::kNormalString, source.size());

    v8::TryCatch tryCatch;
    v8::Handle<v8::Script> script = v8::Script::Compile(src);
    if (catchError(tryCatch, "Compile error", error) || script.IsEmpty())
        return Value();
    v8::Handle<v8::Value> val = script->Run();
    if (catchError(tryCatch, "Evaluate error", error))
        return Value();
    return fromV8(val);
}

void ScriptEngine::throwExceptionInternal(const Value& exception)
{
    v8::Isolate* iso = mPrivate->isolate;
    const v8::Isolate::Scope isolateScope(iso);
    v8::HandleScope handleScope(iso);
    v8::Handle<v8::Value> v8ex = toV8(iso, exception);
    iso->ThrowException(v8ex);
}

ScriptEngine::Object::Object()
    : mPrivate(new ObjectPrivate)
{
}

ScriptEngine::Object::~Object()
{
    delete mPrivate;
}

static void FunctionCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* iso = info.GetIsolate();
    v8::HandleScope handleScope(iso);
    v8::Local<v8::Object> user = v8::Local<v8::Object>::Cast(info.Data());
    String* name = static_cast<String*>(v8::Handle<v8::External>::Cast(user->GetInternalField(0))->Value());

    std::shared_ptr<ScriptEngine::Object> obj = objectFromHolder(info.Holder());
    if (!obj)
        return;

    ObjectPrivate* priv = ObjectPrivate::objectPrivate(obj.get());
    auto func = priv->functions.find(*name);
    if (func == priv->functions.end()) {
        return;
    }
    List<Value> args;
    const auto len = info.Length();
    if (len > 0) {
        args.reserve(len);
        for (auto i = 0; i < len; ++i) {
            args.append(fromV8(info[i]));
        }
    }
    const Value val = func->second.func(args);
    info.GetReturnValue().Set(toV8(iso, val));
}

void ScriptEngine::Object::registerFunction(const String &name, Function &&func)
{
    v8::Isolate* iso = mPrivate->engine->isolate;
    const v8::Isolate::Scope isolateScope(iso);
    v8::HandleScope handleScope(iso);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, mPrivate->engine->context);
    v8::Context::Scope contextScope(ctx);
    v8::Local<v8::Object> obj = v8::Local<v8::Object>::New(iso, mPrivate->object);
    v8::Local<v8::ObjectTemplate> userTempl = v8::ObjectTemplate::New(iso);
    userTempl->SetInternalFieldCount(1);
    v8::Local<v8::Object> user = userTempl->NewInstance();
    String* stringPtr = new String(name);
    user->SetInternalField(0, v8::External::New(iso, stringPtr));
    obj->Set(v8::String::NewFromUtf8(iso, name.constData()),
             v8::Function::New(iso, FunctionCallback, user));

    ObjectPrivate::FunctionData& data = mPrivate->functions[name];
    data.ext.Reset(iso, user);
    data.ext.SetWeak(stringPtr, StringWeak);
    data.ext.MarkIndependent();
    data.func = std::move(func);
}

void ScriptEngine::Object::registerProperty(const String &name, Getter &&get)
{
    ObjectPrivate::PropertyData& data = mPrivate->properties[name];
    data.getter = std::move(get);
    mPrivate->initProperty(name, data, ObjectPrivate::Getter);
}

void ScriptEngine::Object::registerProperty(const String &name, Getter &&get, Setter &&set)
{
    ObjectPrivate::PropertyData& data = mPrivate->properties[name];
    data.getter = std::move(get);
    data.setter = std::move(set);
    mPrivate->initProperty(name, data, ObjectPrivate::Getter|ObjectPrivate::Setter);
}

std::shared_ptr<ScriptEngine::Object> ScriptEngine::Object::child(const String &name)
{
    auto ch = mPrivate->children.find(name);
    if (ch != mPrivate->children.end())
        return ch->second;

    v8::Isolate* iso = mPrivate->engine->isolate;
    const v8::Isolate::Scope isolateScope(iso);
    v8::HandleScope handleScope(iso);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, mPrivate->engine->context);
    v8::Context::Scope contextScope(ctx);
    v8::Local<v8::Object> obj = v8::Local<v8::Object>::New(iso, mPrivate->object);
    v8::Handle<v8::Value> sub = obj->Get(v8::String::NewFromUtf8(iso, name.constData()));
    if (sub.IsEmpty() || sub->IsUndefined()) {
        v8::Handle<v8::ObjectTemplate> templ = v8::ObjectTemplate::New(iso);
        templ->SetInternalFieldCount(1);
        sub = templ->NewInstance();
        //sub = v8::Object::New(iso);
        obj->Set(v8::String::NewFromUtf8(iso, name.constData()), sub);
    }
    if (sub->IsObject()) {
        v8::Handle<v8::Object> subobj = v8::Handle<v8::Object>::Cast(sub);
        std::shared_ptr<Object> ch(new ScriptEngine::Object);
        mPrivate->children[name] = ch;
        ObjectData* data = new ObjectData({ name, ch, shared_from_this() });
        subobj->SetInternalField(0, v8::External::New(iso, data));
        ch->mPrivate->init(mPrivate->engine, subobj);
        return ch;
    }
    return std::shared_ptr<ScriptEngine::Object>();
}

Value ScriptEngine::Object::property(const String &propertyName, String *error)
{
    v8::Isolate* iso = mPrivate->engine->isolate;
    const v8::Isolate::Scope isolateScope(iso);
    v8::HandleScope handleScope(iso);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, mPrivate->engine->context);
    v8::Context::Scope contextScope(ctx);
    v8::Local<v8::Object> obj = v8::Local<v8::Object>::New(iso, mPrivate->object);
    if (obj.IsEmpty()) {
        if (error)
            *error = String::format<128>("Can't find object for property %s", propertyName.constData());
        return Value();
    }

    v8::TryCatch tryCatch;
    v8::Handle<v8::Value> prop = obj->Get(v8::String::NewFromUtf8(iso, propertyName.constData()));
    if (catchError(tryCatch, "Property", error))
        return Value();
    return fromV8(prop);
}

void ScriptEngine::Object::setProperty(const String &propertyName, const Value &value, String *error)
{
    v8::Isolate* iso = mPrivate->engine->isolate;
    const v8::Isolate::Scope isolateScope(iso);
    v8::HandleScope handleScope(iso);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, mPrivate->engine->context);
    v8::Context::Scope contextScope(ctx);
    v8::Local<v8::Object> obj = v8::Local<v8::Object>::New(iso, mPrivate->object);
    if (obj.IsEmpty()) {
        if (error)
            *error = String::format<128>("Can't find object for setProperty %s", propertyName.constData());
        return;
    }

    v8::TryCatch tryCatch;
    obj->Set(v8::String::NewFromUtf8(iso, propertyName.constData()), toV8(iso, value));
    catchError(tryCatch, "Set property", error);
}

Value ScriptEngine::Object::call(const String &functionName, std::initializer_list<Value> arguments, String *error)
{
    v8::Isolate* iso = mPrivate->engine->isolate;
    const v8::Isolate::Scope isolateScope(iso);
    v8::HandleScope handleScope(iso);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, mPrivate->engine->context);
    v8::Context::Scope contextScope(ctx);
    v8::Local<v8::Object> obj = v8::Local<v8::Object>::New(iso, mPrivate->object);
    if (obj.IsEmpty()) {
        if (error)
            *error = String::format<128>("Can't find object for call %s", functionName.constData());
        return Value();
    }

    v8::TryCatch tryCatch;
    v8::Handle<v8::Value> val = obj->Get(v8::String::NewFromUtf8(iso, functionName.constData()));
    if (catchError(tryCatch, "call", error))
        return Value();
    if (!val->IsFunction()) {
        if (error)
            *error = String::format<128>("%s is not a function", functionName.constData());
        return Value();
    }

    v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(val);

    const auto sz = arguments.size();
    List<v8::Handle<v8::Value> > v8args;
    v8args.reserve(sz);
    const Value* arg = arguments.begin();
    const Value* end = arguments.end();
    while (arg != end) {
        v8args.append(toV8(iso, *arg));
        ++arg;
    }

    val = func->Call(obj, sz, &v8args.first());
    if (catchError(tryCatch, "Call error", error))
        return Value();
    return fromV8(val);
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
        return result;
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

static inline v8::Local<v8::Value> toV8_helper(v8::Isolate* isolate, const Value &value)
{
    v8::EscapableHandleScope handleScope(isolate);
    v8::Local<v8::Value> result;
    switch (value.type()) {
    case Value::Type_String:
        result = v8::String::NewFromUtf8(isolate, value.toString().constData());
        break;
    case Value::Type_List: {
        const int sz = value.count();
        v8::Handle<v8::Array> array = v8::Array::New(isolate, sz);
        auto it = value.listBegin();
        for (int i=0; i<sz; ++i) {
            array->Set(i, toV8_helper(isolate, *it));
            ++it;
        }
        result = array;
        break; }
    case Value::Type_Map: {
        v8::Handle<v8::Object> object = v8::Object::New(isolate);
        const auto end = value.end();
        for (auto it = value.begin(); it != end; ++it)
            object->Set(v8::String::NewFromUtf8(isolate, it->first.constData()), toV8_helper(isolate, it->second));
        result = object;
        break; }
    case Value::Type_Integer:
        result = v8::Int32::New(isolate, value.toInt64());
        break;
    case Value::Type_Double:
        result = v8::Number::New(isolate, value.toDouble());
        break;
    case Value::Type_Boolean:
        result = v8::Boolean::New(isolate, value.toBool());
        break;
    default:
        result = v8::Undefined(isolate);
        break;
    }
    return handleScope.Escape(result);
}

static v8::Handle<v8::Value> toV8(v8::Isolate* isolate, const Value& value)
{
    v8::EscapableHandleScope handleScope(isolate);
    return handleScope.Escape(toV8_helper(isolate, value));
}
