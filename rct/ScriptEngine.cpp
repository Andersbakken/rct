#include "ScriptEngine.h"

#ifdef HAVE_SCRIPTENGINE

#include <v8.h>
#include "libplatform/libplatform.h"

#include "rct/EventLoop.h"

static String toString(v8::Handle<v8::Value> value);
static v8::Handle<v8::Value> toV8(v8::Isolate* isolate, const Value& value);
static Value fromV8(v8::Isolate *isolate, v8::Handle<v8::Value> value);

enum CustomType {
    CustomType_Invalid = 0,
    CustomType_Global,
    CustomType_Object,
    CustomType_Function,
    CustomType_AdoptedFunction,
    CustomType_ClassObject
};

struct ScriptEnginePrivate
{
    v8::Persistent<v8::Context> context;
    v8::Isolate *isolate;
#if V8_MAJOR_VERSION > 4 || (V8_MAJOR_VERSION == 4 && V8_MINOR_VERSION >= 9)
    struct ArrayBufferAllocator : public v8::ArrayBuffer::Allocator
    {
        virtual void* Allocate(size_t length) { return calloc(length, 1); }
        virtual void* AllocateUninitialized(size_t length) { return malloc(length); }
        virtual void Free(void* data, size_t /*length*/) { free(data); }
    } arrayBufferAllocator;
#endif

    static ScriptEnginePrivate *get(ScriptEngine *engine) { return engine->mPrivate; }
};

struct ScriptEngineCustom : public Value::Custom
{
    ScriptEngineCustom(int type, v8::Isolate *isolate, const v8::Handle<v8::Object> &obj,
                       const ScriptEngine::Object::SharedPtr &shared)
        : Value::Custom(type), scriptObject(shared)
    {
        assert(shared);
        object.Reset(isolate, obj);
    }

    virtual String toString() const override
    {
        if (object.IsEmpty())
            return String("\"\"");

        ScriptEnginePrivate *engine = ScriptEnginePrivate::get(ScriptEngine::instance());
        v8::Isolate *iso = engine->isolate;
        const v8::Isolate::Scope isolateScope(iso);
        v8::HandleScope handleScope(iso);
        v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, engine->context);
        v8::Context::Scope contextScope(ctx);

        v8::Local<v8::Object> obj = v8::Local<v8::Object>::New(iso, object);
        if (obj.IsEmpty())
            return String("\"\"");
        return ::toString(obj->ToString());
    }

    v8::Persistent<v8::Object> object;
    ScriptEngine::Object::SharedPtr scriptObject;
};

class ObjectPrivate
{
public:
    ObjectPrivate()
        : customType(CustomType_Invalid)
    {
    }
    ~ObjectPrivate()
    {
    }

    enum InitMode { Weak, Persistent };
    void init(CustomType type, ScriptEnginePrivate* e, const v8::Handle<v8::Object>& o, InitMode = Weak);

    struct PropertyData
    {
        ScriptEngine::Getter getter;
        ScriptEngine::Setter setter;
    };
    ScriptEngine::Function func;
    std::function<Value(v8::Handle<v8::Object>, const List<Value> &)> nativeFunc;
    enum { Getter = 0x1, Setter = 0x2 };
    void initProperty(const String& name, PropertyData& data, unsigned int mode);

    CustomType customType;
    ScriptEnginePrivate* engine;
    v8::Persistent<v8::Object> object;
    Hash<String, PropertyData> properties;
    Hash<String, ScriptEngine::Object::SharedPtr > children;
    ScriptEngine::Class::SharedPtr creator;

    // awful
    static ObjectPrivate* objectPrivate(ScriptEngine::Object* obj)
    {
        return obj->mPrivate;
    }

    static ScriptEngine::Object::SharedPtr makeObject()
    {
        return ScriptEngine::Object::SharedPtr(new ScriptEngine::Object);
    }
};

struct ObjectData
{
    ~ObjectData();

    String name;
    std::weak_ptr<ScriptEngine::Object> weak, parent;
};

ObjectData::~ObjectData()
{
    ScriptEngine::Object::SharedPtr obj = weak.lock();
    if (obj) {
        obj->mDestroyed(obj);
    }
}

static inline ScriptEngine::Object::SharedPtr objectFromV8Object(const v8::Local<v8::Object>& holder);
static void functionCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* iso = info.GetIsolate();
    v8::HandleScope handleScope(iso);
    v8::Local<v8::Object> user = v8::Local<v8::Object>::Cast(info.Data());
    ObjectData *data = static_cast<ObjectData*>(v8::Handle<v8::External>::Cast(user->GetInternalField(0))->Value());
    assert(data);

    ScriptEngine::Object::SharedPtr obj = data->weak.lock();
    assert(obj);
    ObjectPrivate* priv = ObjectPrivate::objectPrivate(obj.get());
    assert(priv);
    assert(priv->customType == CustomType_Function);
    assert(priv->func);

    List<Value> args;
    const auto len = info.Length();
    if (len > 0) {
        args.reserve(len);
        for (auto i = 0; i < len; ++i) {
            args.append(fromV8(iso, info[i]));
        }
    }
    const Value val = priv->func(obj, args);
    info.GetReturnValue().Set(toV8(iso, val));
}

static inline ScriptEngine::Object::SharedPtr createObject(const ScriptEngine::Object::SharedPtr &parent, CustomType type, const String &name)
{
    assert(type == CustomType_Object || type == CustomType_Function);
    ObjectPrivate *parentPrivate = ObjectPrivate::objectPrivate(parent.get());
    v8::Isolate *iso = parentPrivate->engine->isolate;
    const v8::Isolate::Scope isolateScope(iso);
    v8::HandleScope handleScope(iso);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, parentPrivate->engine->context);
    v8::Context::Scope contextScope(ctx);

    v8::Handle<v8::Object> subobj;

    v8::Handle<v8::ObjectTemplate> templ = v8::ObjectTemplate::New(iso);
    templ->SetInternalFieldCount(1);
    ScriptEngine::Object::SharedPtr o = ObjectPrivate::makeObject();

    ObjectData* data = new ObjectData({ name, o, parent });
    v8::Local<v8::Object> obj = v8::Local<v8::Object>::New(iso, parentPrivate->object);
    if (type == CustomType_Function) {
        v8::Local<v8::Object> functionData = templ->NewInstance();
        functionData->SetInternalField(0, v8::External::New(iso, data));
        v8::Local<v8::Function> function = v8::Function::New(iso, functionCallback, functionData);
        subobj = function;
    } else {
        v8::Handle<v8::Value> sub = templ->NewInstance();
        subobj = v8::Handle<v8::Object>::Cast(sub);
        subobj->SetHiddenValue(v8::String::NewFromUtf8(iso, "rct"), v8::Int32::New(iso, type));
        subobj->SetInternalField(0, v8::External::New(iso, data));
    }

    obj->Set(v8::String::NewFromUtf8(iso, name.constData()), subobj);

    parentPrivate->children[name] = o;
    ObjectPrivate *priv = ObjectPrivate::objectPrivate(o.get());
    priv->init(type, parentPrivate->engine, subobj);
    return o;
}

static inline ScriptEngine::Object::SharedPtr adoptFunction(v8::Handle<v8::Function> func)
{
    ScriptEnginePrivate *engine = ScriptEnginePrivate::get(ScriptEngine::instance());
    assert(!func.IsEmpty());
    v8::Isolate *iso = engine->isolate;
    const v8::Isolate::Scope isolateScope(iso);
    v8::HandleScope handleScope(iso);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, engine->context);
    v8::Context::Scope contextScope(ctx);

    ScriptEngine::Object::SharedPtr o = ObjectPrivate::makeObject();
    std::weak_ptr<ScriptEngine::Object> weak = o;

    ObjectPrivate *priv = ObjectPrivate::objectPrivate(o.get());

    priv->init(CustomType_AdoptedFunction, engine, func);
    priv->nativeFunc = [weak](v8::Handle<v8::Object> that, const List<Value> &arguments) -> Value {
        ScriptEngine::Object::SharedPtr obj = weak.lock();
        if (!obj)
            return Value();
        ObjectPrivate *priv = ObjectPrivate::objectPrivate(obj.get());
        assert(priv);
        ScriptEnginePrivate *engine = priv->engine;
        v8::Isolate *iso = engine->isolate;
        const v8::Isolate::Scope isolateScope(iso);
        v8::HandleScope handleScope(iso);
        v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, engine->context);
        v8::Context::Scope contextScope(ctx);
        auto v8Obj = v8::Local<v8::Object>::New(iso, priv->object);
        if (v8Obj.IsEmpty() || !v8Obj->IsFunction())
            return Value();
        const auto sz = arguments.size();
        List<v8::Handle<v8::Value> > v8args;
        v8args.reserve(sz);
        for (const auto &arg : arguments) {
            v8args.append(toV8(iso, arg));
        }

        if (that.IsEmpty())
            that = v8Obj;
        v8::TryCatch tryCatch;
        v8::Handle<v8::Value> retVal = v8::Handle<v8::Function>::Cast(v8Obj)->Call(that, sz, v8args.data());
        if (tryCatch.HasCaught()) {
            tryCatch.ReThrow();
            return Value();
        }

        return fromV8(iso, retVal);
    };

    return o;
}

static void ObjectWeak(const v8::WeakCallbackData<v8::Object, ObjectPrivate>& data)
{
    if (data.GetParameter()->customType == CustomType_Global)
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

void ObjectPrivate::init(CustomType type, ScriptEnginePrivate* e, const v8::Handle<v8::Object>& o, InitMode mode)
{
    customType = type;
    engine = e;
    object.Reset(e->isolate, o);
    if (mode == Weak) {
        object.SetWeak(this, ObjectWeak);
        object.MarkIndependent();
    }
}

static inline ScriptEngine::Object::SharedPtr objectFromV8Object(const v8::Local<v8::Object>& holder)
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
        return ScriptEngine::Object::SharedPtr();
    }
    v8::Handle<v8::External> ext = v8::Handle<v8::External>::Cast(val);
    auto data = static_cast<ObjectData*>(ext->Value());
    return data->weak.lock();
}

static void GetterCallback(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    v8::Isolate* iso = info.GetIsolate();
    v8::HandleScope handleScope(iso);

    ScriptEngine::Object::SharedPtr obj = objectFromV8Object(info.Holder());
    if (!obj)
        return;

    ObjectPrivate* priv = ObjectPrivate::objectPrivate(obj.get());
    const String prop = toString(property);
    auto it = priv->properties.find(prop);
    if (it == priv->properties.end())
        return;

    info.GetReturnValue().Set(toV8(iso, it->second.getter(obj)));
}

static void SetterCallback(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
    v8::Isolate* iso = info.GetIsolate();
    v8::HandleScope handleScope(iso);

    ScriptEngine::Object::SharedPtr obj = objectFromV8Object(info.Holder());
    if (!obj)
        return;

    ObjectPrivate* priv = ObjectPrivate::objectPrivate(obj.get());
    const String prop = toString(property);
    auto it = priv->properties.find(prop);
    if (it == priv->properties.end())
        return;

    it->second.setter(obj, fromV8(iso, value));
}

void ObjectPrivate::initProperty(const String& name, PropertyData& /*data*/, unsigned int mode)
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

#if V8_MAJOR_VERSION > 4 || (V8_MAJOR_VERSION == 4 && V8_MINOR_VERSION >= 9)
    v8::V8::InitializeICU();
    const Path exec = Rct::executablePath();
    v8::V8::InitializeExternalStartupData(exec.constData());
    v8::Platform *platform = v8::platform::CreateDefaultPlatform();
    v8::V8::InitializePlatform(platform);
#endif
    v8::V8::Initialize();

#if V8_MAJOR_VERSION > 4 || (V8_MAJOR_VERSION == 4 && V8_MINOR_VERSION >= 9)
    v8::Isolate::CreateParams params;
    params.array_buffer_allocator = &mPrivate->arrayBufferAllocator;
    mPrivate->isolate = v8::Isolate::New(params);
#else
    mPrivate->isolate = v8::Isolate::New();
#endif
    const v8::Isolate::Scope isolateScope(mPrivate->isolate);
    v8::HandleScope handleScope(mPrivate->isolate);
    v8::Handle<v8::ObjectTemplate> globalObjectTemplate = v8::ObjectTemplate::New();

    v8::Handle<v8::Context> ctx = v8::Context::New(mPrivate->isolate, 0, globalObjectTemplate);
    // bind the "global" object to itself
    {
        v8::Context::Scope contextScope(ctx);
        v8::Local<v8::Object> global = ctx->Global();
        global->SetHiddenValue(v8::String::NewFromUtf8(mPrivate->isolate, "rct"), v8::Int32::New(mPrivate->isolate, CustomType_Global));
        global->Set(v8::String::NewFromUtf8(mPrivate->isolate, "global"), global);

        mGlobalObject.reset(new Object);
        mGlobalObject->mPrivate->init(CustomType_Global, mPrivate, global);
    }
    mPrivate->context.Reset(mPrivate->isolate, ctx);
}

ScriptEngine::~ScriptEngine()
{
    mPrivate->isolate->Dispose();
    delete mPrivate;
    assert(sInstance == this);
    sInstance = 0;

    v8::V8::Dispose();
#if V8_MAJOR_VERSION > 4 || (V8_MAJOR_VERSION == 4 && V8_MINOR_VERSION >= 9)
    v8::V8::ShutdownPlatform();
#endif
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
    return fromV8(mPrivate->isolate, val);
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
    val = func->Call(that, sz, v8args.data());
    if (catchError(tryCatch, "Call error", error))
        return Value();
    return fromV8(mPrivate->isolate, val);
}

Value ScriptEngine::evaluate(const String &source, const Path &path, String *error)
{
    const v8::Isolate::Scope isolateScope(mPrivate->isolate);
    v8::HandleScope handleScope(mPrivate->isolate);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(mPrivate->isolate, mPrivate->context);
    v8::Context::Scope contextScope(ctx);
    v8::Handle<v8::String> src = v8::String::NewFromUtf8(mPrivate->isolate, source.constData(), v8::String::kNormalString, source.size());
    v8::Handle<v8::String> fn = v8::String::NewFromUtf8(mPrivate->isolate, path.constData());

    v8::TryCatch tryCatch;
    v8::Handle<v8::Script> script = v8::Script::Compile(src, fn);
    if (catchError(tryCatch, "Compile error", error) || script.IsEmpty())
        return Value();
    v8::Handle<v8::Value> val = script->Run();
    if (catchError(tryCatch, "Evaluate error", error))
        return Value();
    return fromV8(mPrivate->isolate, val);
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
    : mPrivate(new ObjectPrivate), mData(0)
{
}

ScriptEngine::Object::~Object()
{
    delete mPrivate;
    delete mData;
}

bool ScriptEngine::Object::isFunction() const
{
    return (mPrivate->customType == CustomType_Function
            || mPrivate->customType == CustomType_AdoptedFunction);
}

ScriptEngine::Object::SharedPtr ScriptEngine::Object::registerFunction(const String &name, Function &&func)
{
    ScriptEngine::Object::SharedPtr obj = ::createObject(shared_from_this(), CustomType_Function, name);
    assert(obj);
    obj->mPrivate->func = std::move(func);
    return obj;
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

ScriptEngine::Object::SharedPtr ScriptEngine::Object::child(const String &name)
{
    auto ch = mPrivate->children.find(name);
    if (ch != mPrivate->children.end())
        return ch->second;

    return ::createObject(shared_from_this(), CustomType_Object, name);
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
    return fromV8(iso, prop);
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

Value ScriptEngine::Object::call(std::initializer_list<Value> arguments,
                                 const ScriptEngine::Object::SharedPtr &/*thisObject*/,
                                 String *error)
{
    assert(mPrivate->customType == CustomType_Function || mPrivate->customType == CustomType_AdoptedFunction);
    if (mPrivate->customType == CustomType_Function) {
        return mPrivate->func(shared_from_this(), arguments);
    }

    v8::Isolate* iso = mPrivate->engine->isolate;
    const v8::Isolate::Scope isolateScope(iso);
    v8::HandleScope handleScope(iso);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, mPrivate->engine->context);
    v8::Context::Scope contextScope(ctx);
    v8::Local<v8::Object> obj = v8::Local<v8::Object>::New(iso, mPrivate->object);
    if (obj.IsEmpty()) {
        if (error)
            *error = String::format<128>("Can't find object for call");
        return Value();
    }

    v8::TryCatch tryCatch;
    assert(obj->IsFunction());
    v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(obj);

    const auto sz = arguments.size();
    List<v8::Handle<v8::Value> > v8args;
    v8args.reserve(sz);
    const Value* arg = arguments.begin();
    const Value* end = arguments.end();
    while (arg != end) {
        v8args.append(toV8(iso, *arg));
        ++arg;
    }

    v8::Handle<v8::Value> val = func->Call(obj, sz, v8args.data());
    if (catchError(tryCatch, "Call error", error))
        return Value();
    return fromV8(iso, val);
}

Value ScriptEngine::fromObject(const Object::SharedPtr& object)
{
    ObjectPrivate* priv = ObjectPrivate::objectPrivate(object.get());

    v8::Isolate* iso = priv->engine->isolate;
    const v8::Isolate::Scope isolateScope(iso);
    v8::HandleScope handleScope(iso);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, priv->engine->context);
    v8::Context::Scope contextScope(ctx);

    v8::Local<v8::Object> obj = v8::Local<v8::Object>::New(iso, priv->object);
    if (obj.IsEmpty()) {
        error() << "unable to lock persistent for fromObject";
        return Value();
    }

    return Value(std::make_shared<ScriptEngineCustom>(priv->customType, iso, obj, object));
}

ScriptEngine::Object::SharedPtr ScriptEngine::toObject(const Value &value) const
{
    const std::shared_ptr<ScriptEngineCustom> &custom = std::static_pointer_cast<ScriptEngineCustom>(value.toCustom());
    if (!custom || custom->object.IsEmpty()) {
        return ScriptEngine::Object::SharedPtr();
    }
    return custom->scriptObject;
}

static String toString(v8::Handle<v8::Value> value)
{
    if (value->IsString()) {
        v8::String::Utf8Value strValue(value);
        return String(*strValue);
    }
    return String();
}

static Value fromV8(v8::Isolate *isolate, v8::Handle<v8::Value> value)
{
    if (value->IsString()) {
        return toString(value);
    } else if (value->IsDate()) {
        v8::Handle<v8::Date> date = v8::Handle<v8::Date>::Cast(value);
        return Date(static_cast<uint64_t>(date->ValueOf() / 1000));
    } else if (value->IsArray()) {
        v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(value);
        List<Value> result(array->Length());
        for (size_t i = 0; i < array->Length(); ++i)
            result[i] = fromV8(isolate, array->Get(i));
        return result;
    } else if (value->IsObject()) {
        v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
        v8::Handle<v8::Value> rct = object->GetHiddenValue(v8::String::NewFromUtf8(isolate, "rct"));
        if (!rct.IsEmpty() && rct->IsInt32()) {
            return Value(std::make_shared<ScriptEngineCustom>(rct->ToInt32()->Value(), isolate,
                                                              object, objectFromV8Object(object)));
        } else if (object->IsFunction()) {
            return Value(std::make_shared<ScriptEngineCustom>(CustomType_AdoptedFunction, isolate, object,
                                                              adoptFunction(v8::Handle<v8::Function>::Cast(object))));
        }
        Value result;
        v8::Local<v8::Array> properties = object->GetOwnPropertyNames();
        for(size_t i = 0; i < properties->Length(); ++i) {
            const v8::Handle<v8::Value> key = properties->Get(i);
            result[toString(key)] = fromV8(isolate, object->Get(key));
        }
        return result;
    } else if (value->IsInt32()) {
        return value->IntegerValue();
    } else if (value->IsNumber()) {
        return value->NumberValue();
    } else if (value->IsBoolean()) {
        return value->BooleanValue();
    } if (value->IsUndefined()) {
        return Value::undefined();
    } else if (!value->IsNull() && !value->IsUndefined()) {
        error() << "Unknown value type in fromV8";
    }
    return Value();
}

static inline v8::Local<v8::Value> toV8_helper(v8::Isolate* isolate, const Value &value)
{
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
    case Value::Type_Custom: {
        const std::shared_ptr<ScriptEngineCustom> &custom = std::static_pointer_cast<ScriptEngineCustom>(value.toCustom());
        if (!custom || custom->object.IsEmpty()) {
            result = v8::Undefined(isolate);
        } else {
            result = v8::Local<v8::Object>::New(isolate, custom->object);
        }
        break; }
    case Value::Type_Integer:
        result = v8::Int32::New(isolate, value.toLongLong());
        break;
    case Value::Type_Double:
        result = v8::Number::New(isolate, value.toDouble());
        break;
    case Value::Type_Boolean:
        result = v8::Boolean::New(isolate, value.toBool());
        break;
    case Value::Type_Undefined:
        result = v8::Undefined(isolate);
        break;
    case Value::Type_Date:
        result = v8::Date::New(isolate, static_cast<double>(value.toLongLong() * 1000));
        break;
    default:
        break;
    }
    return result;
}

static v8::Handle<v8::Value> toV8(v8::Isolate* isolate, const Value& value)
{
    v8::EscapableHandleScope handleScope(isolate);
    return handleScope.Escape(toV8_helper(isolate, value));
}

class ClassPrivate
{
public:
    struct PropertyData
    {
        ScriptEngine::Getter getter;
        ScriptEngine::Setter setter;
    };
    enum { Getter = 0x1, Setter = 0x2 };
    void initProperty(const String& name, PropertyData& data, unsigned int mode);
    struct FunctionData
    {
        ScriptEngine::Function function;
        v8::Persistent<v8::FunctionTemplate> templ;
    };
    struct
    {
        ScriptEngine::Class::InterceptGet getter;
        ScriptEngine::Class::InterceptSet setter;
        ScriptEngine::Class::InterceptQuery query;
        ScriptEngine::Class::InterceptQuery deleter;
        ScriptEngine::Class::InterceptEnumerate enumerator;
    } intercept;

    ScriptEnginePrivate* engine;
    v8::Persistent<v8::FunctionTemplate> functionTempl, ctorTempl;
    Hash<String, FunctionData> functions;
    Hash<String, ScriptEngine::StaticFunction> staticFunctions;
    Hash<String, PropertyData> properties;
    ScriptEngine::Class::Constructor constructor;
    ScriptEngine::Class::WeakPtr cls;

    static ClassPrivate* classPrivate(ScriptEngine::Class* cls)
    {
        return cls->mPrivate;
    }

    ScriptEngine::Object::SharedPtr create();
};

ScriptEngine::Object::SharedPtr ScriptEngine::createObject() const
{
    v8::Isolate* iso = mPrivate->isolate;

    const v8::Isolate::Scope isolateScope(iso);
    v8::HandleScope handleScope(iso);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, mPrivate->context);
    v8::Context::Scope contextScope(ctx);

    v8::Local<v8::ObjectTemplate> otempl = v8::ObjectTemplate::New(iso);
    otempl->SetInternalFieldCount(1);

    v8::Handle<v8::Object> obj = otempl->NewInstance();
    ScriptEngine::Object::SharedPtr o = ObjectPrivate::makeObject();

    ObjectData* data = new ObjectData({ String(), o, ScriptEngine::Object::SharedPtr() });
    obj->SetHiddenValue(v8::String::NewFromUtf8(iso, "rct"), v8::Int32::New(iso, CustomType_ClassObject));
    obj->SetInternalField(0, v8::External::New(iso, data));

    ObjectPrivate *priv = ObjectPrivate::objectPrivate(o.get());
    priv->init(CustomType_ClassObject, mPrivate, obj, ObjectPrivate::Persistent);

    return o;
}

ScriptEngine::Object::SharedPtr ClassPrivate::create()
{
    ScriptEngine::Class::SharedPtr ptr = cls.lock();
    assert(ptr);

    v8::Isolate* iso = engine->isolate;
    const v8::Isolate::Scope isolateScope(iso);
    v8::HandleScope handleScope(iso);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, engine->context);
    v8::Context::Scope contextScope(ctx);

    v8::Local<v8::FunctionTemplate> templ = v8::Local<v8::FunctionTemplate>::New(iso, functionTempl);
    v8::Handle<v8::Object> obj = templ->GetFunction()->NewInstance();
    ScriptEngine::Object::SharedPtr o = ObjectPrivate::makeObject();

    ObjectData* data = new ObjectData({ String(), o, ScriptEngine::Object::SharedPtr() });
    obj->SetHiddenValue(v8::String::NewFromUtf8(iso, "rct"), v8::Int32::New(iso, CustomType_ClassObject));
    obj->SetInternalField(0, v8::External::New(iso, data));

    ObjectPrivate *priv = ObjectPrivate::objectPrivate(o.get());
    priv->creator = ptr;
    priv->init(CustomType_ClassObject, engine, obj);

    return o;
}

static void ClassGetterCallback(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    v8::Isolate* iso = info.GetIsolate();
    v8::HandleScope handleScope(iso);

    ScriptEngine::Object::SharedPtr obj = objectFromV8Object(info.Holder());
    if (!obj)
        return;

    ObjectPrivate* objpriv = ObjectPrivate::objectPrivate(obj.get());
    ScriptEngine::Class::SharedPtr cls = objpriv->creator;
    assert(cls);
    ClassPrivate* priv = ClassPrivate::classPrivate(cls.get());
    const String prop = toString(property);
    auto it = priv->properties.find(prop);
    if (it == priv->properties.end())
        return;

    info.GetReturnValue().Set(toV8(iso, it->second.getter(obj)));
}

static void ClassFunctionPropertyCallback(v8::Local<v8::String> function, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    v8::Isolate* iso = info.GetIsolate();
    v8::HandleScope handleScope(iso);

    ScriptEngine::Object::SharedPtr obj = objectFromV8Object(info.Holder());
    if (!obj)
        return;

    ObjectPrivate* objpriv = ObjectPrivate::objectPrivate(obj.get());
    ScriptEngine::Class::SharedPtr cls = objpriv->creator;
    assert(cls);
    ClassPrivate* priv = ClassPrivate::classPrivate(cls.get());
    const String func = toString(function);
    auto it = priv->functions.find(func);
    if (it == priv->functions.end())
        return;

    v8::Local<v8::FunctionTemplate> sub = v8::Local<v8::FunctionTemplate>::New(iso, it->second.templ);
    info.GetReturnValue().Set(sub->GetFunction());
}

static void ClassSetterCallback(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
    v8::Isolate* iso = info.GetIsolate();
    v8::HandleScope handleScope(iso);

    ScriptEngine::Object::SharedPtr obj = objectFromV8Object(info.Holder());
    if (!obj)
        return;

    ObjectPrivate* objpriv = ObjectPrivate::objectPrivate(obj.get());
    ScriptEngine::Class::SharedPtr cls = objpriv->creator;
    assert(cls);
    ClassPrivate* priv = ClassPrivate::classPrivate(cls.get());
    const String prop = toString(property);
    auto it = priv->properties.find(prop);
    if (it == priv->properties.end())
        return;

    it->second.setter(obj, fromV8(iso, value));
}

void ClassPrivate::initProperty(const String& name, PropertyData& /*data*/, unsigned int mode)
{
    v8::Isolate* iso = engine->isolate;
    const v8::Isolate::Scope isolateScope(iso);
    v8::HandleScope handleScope(iso);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, engine->context);
    v8::Context::Scope contextScope(ctx);
    v8::Local<v8::FunctionTemplate> templ = v8::Local<v8::FunctionTemplate>::New(iso, functionTempl);
    assert(mode & Getter);
    if (mode & Setter) {
        templ->InstanceTemplate()->SetAccessor(v8::String::NewFromUtf8(iso, name.constData()),
                                               ClassGetterCallback,
                                               ClassSetterCallback);
    } else {
        templ->InstanceTemplate()->SetAccessor(v8::String::NewFromUtf8(iso, name.constData()),
                                               ClassGetterCallback);
    }
}

static void ClassConstruct(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* iso = info.GetIsolate();
    v8::HandleScope handleScope(iso);

    v8::Handle<v8::External> ext = v8::Handle<v8::External>::Cast(v8::Handle<v8::Object>::Cast(info.Data())->GetInternalField(0));
    ClassPrivate* priv = static_cast<ClassPrivate*>(ext->Value());
    if (!priv->constructor)
        return;
    List<Value> args;
    const auto len = info.Length();
    if (len > 0) {
        args.reserve(len);
        for (auto i = 0; i < len; ++i) {
            args.append(fromV8(iso, info[i]));
        }
    }
    Value val = priv->constructor(args);
    if (!val.isCustom())
        return;
    v8::Local<v8::Value> v8obj = toV8(iso, val);
    if (v8obj.IsEmpty() || !v8obj->IsObject()) {
        v8::Local<v8::String> ex = v8::String::NewFromUtf8(iso, "Unable to get object for ClassConstruct");
        iso->ThrowException(ex);
        return;
    }

    info.GetReturnValue().Set(v8obj);
}

ScriptEngine::Class::Class(const String& name)
    : mPrivate(new ClassPrivate)
{
    ScriptEnginePrivate* engine = ScriptEnginePrivate::get(ScriptEngine::instance());
    mPrivate->engine = engine;
    v8::Isolate* iso = engine->isolate;
    const v8::Isolate::Scope isolateScope(iso);
    v8::HandleScope handleScope(iso);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, engine->context);
    v8::Context::Scope contextScope(ctx);

    v8::Local<v8::FunctionTemplate> ftempl = v8::FunctionTemplate::New(iso);

    ftempl->InstanceTemplate()->SetInternalFieldCount(1);
    ftempl->SetClassName(v8::String::NewFromUtf8(iso, name.constData()));
    mPrivate->functionTempl.Reset(iso, ftempl);

    v8::Local<v8::ObjectTemplate> cdtempl = v8::ObjectTemplate::New(iso);
    cdtempl->SetInternalFieldCount(1);
    v8::Local<v8::Object> cdata = cdtempl->NewInstance();
    cdata->SetInternalField(0, v8::External::New(iso, mPrivate));
    v8::Local<v8::FunctionTemplate> ctempl = v8::FunctionTemplate::New(iso, ClassConstruct, cdata);
    ctempl->SetClassName(v8::String::NewFromUtf8(iso, name.constData()));
    mPrivate->ctorTempl.Reset(iso, ctempl);

    // expose to JS
    v8::Local<v8::Object> global = ctx->Global();
    global->Set(v8::String::NewFromUtf8(iso, name.constData()), ctempl->GetFunction());
}

ScriptEngine::Class::~Class()
{
    delete mPrivate;
}

static void ClassFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* iso = info.GetIsolate();
    v8::HandleScope handleScope(iso);

    ScriptEngine::Object::SharedPtr obj = objectFromV8Object(info.Holder());
    if (!obj)
        return;

    v8::Local<v8::Object> data = v8::Local<v8::Object>::Cast(info.Data());
    ClassPrivate* priv = static_cast<ClassPrivate*>(v8::Local<v8::External>::Cast(data->GetInternalField(0))->Value());
    const String name = toString(v8::Local<v8::String>::Cast(data->GetInternalField(1)));

    auto it = priv->functions.find(name);
    if (it == priv->functions.end())
        return;

    List<Value> args;
    const auto len = info.Length();
    if (len > 0) {
        args.reserve(len);
        for (auto i = 0; i < len; ++i) {
            args.append(fromV8(iso, info[i]));
        }
    }
    const Value val = it->second.function(obj, args);
    info.GetReturnValue().Set(toV8(iso, val));
}

static void ClassStaticFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* iso = info.GetIsolate();
    v8::HandleScope handleScope(iso);

    v8::Local<v8::Object> data = v8::Local<v8::Object>::Cast(info.Data());
    ClassPrivate* priv = static_cast<ClassPrivate*>(v8::Local<v8::External>::Cast(data->GetInternalField(0))->Value());
    const String name = toString(v8::Local<v8::String>::Cast(data->GetInternalField(1)));

    auto it = priv->staticFunctions.find(name);
    if (it == priv->staticFunctions.end())
        return;

    List<Value> args;
    const auto len = info.Length();
    if (len > 0) {
        args.reserve(len);
        for (auto i = 0; i < len; ++i) {
            args.append(fromV8(iso, info[i]));
        }
    }
    const Value val = it->second(args);
    info.GetReturnValue().Set(toV8(iso, val));
}

void ScriptEngine::Class::registerFunction(const String &name, Function &&func)
{
    ScriptEnginePrivate* engine = mPrivate->engine;
    v8::Isolate* iso = engine->isolate;
    const v8::Isolate::Scope isolateScope(iso);
    v8::HandleScope handleScope(iso);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, engine->context);
    v8::Context::Scope contextScope(ctx);

    ClassPrivate::FunctionData& data = mPrivate->functions[name];
    data.function = std::move(func);
    // geh
    v8::Handle<v8::ObjectTemplate> objTempl = v8::ObjectTemplate::New(iso);
    objTempl->SetInternalFieldCount(2);
    v8::Handle<v8::Object> ext = objTempl->NewInstance();
    ext->SetInternalField(0, v8::External::New(iso, mPrivate));
    ext->SetInternalField(1, v8::String::NewFromUtf8(iso, name.constData()));
    v8::Handle<v8::FunctionTemplate> funcTempl = v8::FunctionTemplate::New(iso, ClassFunctionCallback, ext);
    data.templ.Reset(iso, funcTempl);

    v8::Local<v8::FunctionTemplate> templ = v8::Local<v8::FunctionTemplate>::New(iso, mPrivate->functionTempl);
    templ->InstanceTemplate()->SetAccessor(v8::String::NewFromUtf8(iso, name.constData()),
                                           ClassFunctionPropertyCallback);

}

void ScriptEngine::Class::registerStaticFunction(const String &name, StaticFunction &&func)
{
    ScriptEnginePrivate* engine = mPrivate->engine;
    v8::Isolate* iso = engine->isolate;
    const v8::Isolate::Scope isolateScope(iso);
    v8::HandleScope handleScope(iso);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, engine->context);
    v8::Context::Scope contextScope(ctx);

    mPrivate->staticFunctions[name] = std::move(func);
    // geh
    v8::Handle<v8::ObjectTemplate> objTempl = v8::ObjectTemplate::New(iso);
    objTempl->SetInternalFieldCount(2);
    v8::Handle<v8::Object> ext = objTempl->NewInstance();
    ext->SetInternalField(0, v8::External::New(iso, mPrivate));
    ext->SetInternalField(1, v8::String::NewFromUtf8(iso, name.constData()));
    v8::Handle<v8::Function> function = v8::Function::New(iso, ClassStaticFunctionCallback, ext);

    v8::Local<v8::FunctionTemplate> templ = v8::Local<v8::FunctionTemplate>::New(iso, mPrivate->ctorTempl);

    templ->GetFunction()->Set(v8::String::NewFromUtf8(iso, name.constData()), function);
}

void ScriptEngine::Class::registerProperty(const String &name, Getter &&get)
{
    ClassPrivate::PropertyData& data = mPrivate->properties[name];
    data.getter = std::move(get);
    mPrivate->initProperty(name, data, ClassPrivate::Getter);
}

void ScriptEngine::Class::registerProperty(const String &name, Getter &&get, Setter &&set)
{
    ClassPrivate::PropertyData& data = mPrivate->properties[name];
    data.getter = std::move(get);
    data.setter = std::move(set);
    mPrivate->initProperty(name, data, ClassPrivate::Getter|ClassPrivate::Setter);
}

void ScriptEngine::Class::registerConstructor(Constructor&& ctor)
{
    mPrivate->constructor = std::move(ctor);
}

void ScriptEngine::Class::init()
{
    mPrivate->cls = shared_from_this();
}

ScriptEngine::Object::SharedPtr ScriptEngine::Class::create()
{
    return mPrivate->create();
}

static void ClassInterceptGetter(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    v8::Isolate* iso = info.GetIsolate();
    const v8::Isolate::Scope isolateScope(iso);
    ScriptEngine::Object::SharedPtr obj = objectFromV8Object(info.Holder());
    if (!obj)
        return;

    v8::HandleScope handleScope(iso);
    v8::Handle<v8::External> ext = v8::Handle<v8::External>::Cast(v8::Handle<v8::Object>::Cast(info.Data())->GetInternalField(0));
    ClassPrivate* priv = static_cast<ClassPrivate*>(ext->Value());
    ScriptEnginePrivate* engine = priv->engine;
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, engine->context);
    v8::Context::Scope contextScope(ctx);

    const Value r = priv->intercept.getter(obj, toString(property));
    if (r.type() == Value::Type_Invalid)
        return;
    info.GetReturnValue().Set(toV8(iso, r));
}

static void ClassInterceptSetter(v8::Local<v8::String> property, v8::Local<v8::Value> value,
                                 const v8::PropertyCallbackInfo<v8::Value>& info)
{
    v8::Isolate* iso = info.GetIsolate();
    const v8::Isolate::Scope isolateScope(iso);
    ScriptEngine::Object::SharedPtr obj = objectFromV8Object(info.Holder());
    if (!obj)
        return;

    v8::HandleScope handleScope(iso);
    v8::Handle<v8::External> ext = v8::Handle<v8::External>::Cast(v8::Handle<v8::Object>::Cast(info.Data())->GetInternalField(0));
    ClassPrivate* priv = static_cast<ClassPrivate*>(ext->Value());
    ScriptEnginePrivate* engine = priv->engine;
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, engine->context);
    v8::Context::Scope contextScope(ctx);

    const Value v = fromV8(iso, value);
    const Value r = priv->intercept.setter(obj, toString(property), v);
    if (r.type() == Value::Type_Invalid)
        return;
    info.GetReturnValue().Set(toV8(iso, r));
}

static void ClassInterceptQuery(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Integer>& info)
{
    v8::Isolate* iso = info.GetIsolate();
    const v8::Isolate::Scope isolateScope(iso);

    v8::HandleScope handleScope(iso);
    v8::Handle<v8::External> ext = v8::Handle<v8::External>::Cast(v8::Handle<v8::Object>::Cast(info.Data())->GetInternalField(0));
    ClassPrivate* priv = static_cast<ClassPrivate*>(ext->Value());
    ScriptEnginePrivate* engine = priv->engine;
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, engine->context);
    v8::Context::Scope contextScope(ctx);

    abort();
    const Value r = priv->intercept.query(toString(property));
    if (r.type() != Value::Type_Integer)
        return;
    info.GetReturnValue().Set(v8::Integer::New(iso, r.toInteger()));
}

static void ClassInterceptDeleter(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Boolean>& info)
{
    v8::Isolate* iso = info.GetIsolate();
    const v8::Isolate::Scope isolateScope(iso);

    v8::HandleScope handleScope(iso);
    v8::Handle<v8::External> ext = v8::Handle<v8::External>::Cast(v8::Handle<v8::Object>::Cast(info.Data())->GetInternalField(0));
    ClassPrivate* priv = static_cast<ClassPrivate*>(ext->Value());
    ScriptEnginePrivate* engine = priv->engine;
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, engine->context);
    v8::Context::Scope contextScope(ctx);

    const Value r = priv->intercept.deleter(toString(property));
    if (r.type() != Value::Type_Boolean)
        return;
    info.GetReturnValue().Set(r.toBool() ? v8::True(iso) : v8::False(iso));
}

static void ClassInterceptEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info)
{
    v8::Isolate* iso = info.GetIsolate();
    const v8::Isolate::Scope isolateScope(iso);

    v8::HandleScope handleScope(iso);
    v8::Handle<v8::External> ext = v8::Handle<v8::External>::Cast(v8::Handle<v8::Object>::Cast(info.Data())->GetInternalField(0));
    ClassPrivate* priv = static_cast<ClassPrivate*>(ext->Value());
    ScriptEnginePrivate* engine = priv->engine;
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, engine->context);
    v8::Context::Scope contextScope(ctx);

    const Value r = priv->intercept.enumerator();
    if (r.type() != Value::Type_List)
        return;
    const List<Value> l = r.toList();
    v8::Local<v8::Array> array = v8::Array::New(iso, l.size());
    for (size_t idx = 0; idx < l.size(); ++idx)
        array->Set(idx, toV8(iso, l[idx]));
    info.GetReturnValue().Set(array);
}

void ScriptEngine::Class::interceptPropertyName(InterceptGet&& get,
                                                InterceptSet&& set,
                                                InterceptQuery&& query,
                                                InterceptQuery&& deleter,
                                                InterceptEnumerate&& enumerator)
{
    mPrivate->intercept.getter = std::move(get);
    mPrivate->intercept.setter = std::move(set);
    mPrivate->intercept.query = std::move(query);
    mPrivate->intercept.deleter = std::move(deleter);
    mPrivate->intercept.enumerator = std::move(enumerator);

    ScriptEnginePrivate* engine = mPrivate->engine;
    v8::Isolate* iso = engine->isolate;
    const v8::Isolate::Scope isolateScope(iso);
    v8::HandleScope handleScope(iso);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, engine->context);
    v8::Context::Scope contextScope(ctx);

    v8::Local<v8::FunctionTemplate> templ = v8::Local<v8::FunctionTemplate>::New(iso, mPrivate->functionTempl);
    v8::Handle<v8::ObjectTemplate> objTempl = v8::ObjectTemplate::New(iso);
    objTempl->SetInternalFieldCount(1);
    v8::Handle<v8::Object> data = objTempl->NewInstance();
    data->SetInternalField(0, v8::External::New(iso, mPrivate));
    templ->InstanceTemplate()->SetNamedPropertyHandler(ClassInterceptGetter,
                                                       ClassInterceptSetter,
                                                       ClassInterceptQuery,
                                                       ClassInterceptDeleter,
                                                       ClassInterceptEnumerator,
                                                       data);

    if (!mPrivate->functions.contains("toString")) {
        registerFunction("toString", [](const Object::SharedPtr &obj, const List<Value> &) -> Value {
                ScriptEnginePrivate *engine = ScriptEnginePrivate::get(ScriptEngine::instance());
                v8::Isolate *iso = engine->isolate;
                const v8::Isolate::Scope isolateScope(iso);
                v8::HandleScope handleScope(iso);
                v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, engine->context);
                v8::Context::Scope contextScope(ctx);
                ObjectPrivate* priv = ObjectPrivate::objectPrivate(obj.get());
                assert(priv);
                auto globalObject = ctx->Global();
                assert(!globalObject.IsEmpty());
                auto json = globalObject->Get(v8::String::NewFromUtf8(iso, "JSON"));
                assert(!json.IsEmpty());
                assert(json->IsObject());
                auto jsonObject = v8::Local<v8::Object>::Cast(json);
                auto stringify = jsonObject->Get(v8::String::NewFromUtf8(iso, "stringify"));
                assert(!stringify.IsEmpty());
                assert(stringify->IsFunction());

                v8::TryCatch tryCatch;
                v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(stringify);
                v8::Local<v8::Object> objValue = v8::Local<v8::Object>::New(iso, priv->object);
                v8::Local<v8::Value> value = objValue;
                auto result = func->Call(json, 1, &value);
                if (tryCatch.HasCaught()) {
                    return "\"[object Object]\"";
                }
                return fromV8(iso, result);
            });
    }
}
bool ScriptEngine::isFunction(const Value &value) const
{
    if (auto object = toObject(value))
        return object->isFunction();
    return false;
}

#endif
