#include "ScriptEngine.h"

#include <v8.h>
#include <rct/EventLoop.h>

static String toString(v8::Handle<v8::Value> value);
static v8::Handle<v8::Value> toV8(v8::Isolate* isolate, const Value& value);
static Value fromV8(v8::Isolate *isolate, v8::Handle<v8::Value> value);

enum CustomType {
    CustomType_Invalid = 0,
    CustomType_Global,
    CustomType_Object,
    CustomType_Function,
    CustomType_AdoptedFunction
};

struct ScriptEngineCustom : public Value::Custom
{
    ScriptEngineCustom(int type, v8::Isolate *isolate, const v8::Handle<v8::Object> &obj,
                       const std::shared_ptr<ScriptEngine::Object> &shared)
        : Value::Custom(type), scriptObject(shared)
    {
        assert(shared);
        object.Reset(isolate, obj);
    }

    v8::Persistent<v8::Object> object;
    std::shared_ptr<ScriptEngine::Object> scriptObject;
};

struct ScriptEnginePrivate
{
    v8::Persistent<v8::Context> context;
    v8::Isolate *isolate;

    static ScriptEnginePrivate *get(ScriptEngine *engine) { return engine->mPrivate; }
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

    void init(CustomType type, ScriptEnginePrivate* e, const v8::Handle<v8::Object>& o);

    struct PropertyData
    {
        ScriptEngine::Object::Getter getter;
        ScriptEngine::Object::Setter setter;
    };
    ScriptEngine::Object::Function func;
    std::function<Value(v8::Handle<v8::Object>, const List<Value> &)> nativeFunc;
    enum { Getter = 0x1, Setter = 0x2 };
    void initProperty(const String& name, PropertyData& data, unsigned int mode);

    CustomType customType;
    ScriptEnginePrivate* engine;
    v8::Persistent<v8::Object> object;
    Hash<String, PropertyData> properties;
    Hash<String, std::shared_ptr<ScriptEngine::Object> > children;
    Value extraData;

    // awful
    static ObjectPrivate* objectPrivate(ScriptEngine::Object* obj)
    {
        return obj->mPrivate;
    }

    static std::shared_ptr<ScriptEngine::Object> makeObject()
    {
        return std::shared_ptr<ScriptEngine::Object>(new ScriptEngine::Object);
    }
};

struct ObjectData
{
    String name;
    std::weak_ptr<ScriptEngine::Object> weak, parent;
};

static inline std::shared_ptr<ScriptEngine::Object> objectFromV8Object(const v8::Local<v8::Object>& holder);
static void functionCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* iso = info.GetIsolate();
    v8::HandleScope handleScope(iso);
    v8::Local<v8::Object> user = v8::Local<v8::Object>::Cast(info.Data());
    ObjectData *data = static_cast<ObjectData*>(v8::Handle<v8::External>::Cast(user->GetInternalField(0))->Value());
    assert(data);

    std::shared_ptr<ScriptEngine::Object> obj = data->weak.lock();
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
    const Value val = priv->func(args);
    info.GetReturnValue().Set(toV8(iso, val));
}

static inline std::shared_ptr<ScriptEngine::Object> createObject(const std::shared_ptr<ScriptEngine::Object> &parent, CustomType type, const String &name)
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
    std::shared_ptr<ScriptEngine::Object> o = ObjectPrivate::makeObject();

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

static inline std::shared_ptr<ScriptEngine::Object> adoptFunction(v8::Handle<v8::Function> func)
{
    ScriptEnginePrivate *engine = ScriptEnginePrivate::get(ScriptEngine::instance());
    assert(!func.IsEmpty());
    v8::Isolate *iso = engine->isolate;
    const v8::Isolate::Scope isolateScope(iso);
    v8::HandleScope handleScope(iso);
    v8::Local<v8::Context> ctx = v8::Local<v8::Context>::New(iso, engine->context);
    v8::Context::Scope contextScope(ctx);

    std::shared_ptr<ScriptEngine::Object> o = ObjectPrivate::makeObject();
    std::weak_ptr<ScriptEngine::Object> weak = o;

    ObjectPrivate *priv = ObjectPrivate::objectPrivate(o.get());

    priv->init(CustomType_AdoptedFunction, engine, func);
    priv->nativeFunc = [weak](v8::Handle<v8::Object> that, const List<Value> &arguments) -> Value {
        std::shared_ptr<ScriptEngine::Object> obj = weak.lock();
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

void ObjectPrivate::init(CustomType type, ScriptEnginePrivate* e, const v8::Handle<v8::Object>& o)
{
    customType = type;
    engine = e;
    object.Reset(e->isolate, o);
    object.SetWeak(this, ObjectWeak);
    object.MarkIndependent();
}

static inline std::shared_ptr<ScriptEngine::Object> objectFromV8Object(const v8::Local<v8::Object>& holder)
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

    std::shared_ptr<ScriptEngine::Object> obj = objectFromV8Object(info.Holder());
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

    std::shared_ptr<ScriptEngine::Object> obj = objectFromV8Object(info.Holder());
    if (!obj)
        return;

    ObjectPrivate* priv = ObjectPrivate::objectPrivate(obj.get());
    const String prop = toString(property);
    auto it = priv->properties.find(prop);
    if (it == priv->properties.end())
        return;

    it->second.setter(fromV8(iso, value));
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
        global->SetHiddenValue(v8::String::NewFromUtf8(mPrivate->isolate, "rct"), v8::Int32::New(mPrivate->isolate, CustomType_Global));
        global->Set(v8::String::NewFromUtf8(mPrivate->isolate, "global"), global);

        mGlobalObject.reset(new Object);
        mGlobalObject->mPrivate->init(CustomType_Global, mPrivate, global);
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

    v8::TryCatch tryCatch;
    v8::Handle<v8::Script> script = v8::Script::Compile(src);
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
    : mPrivate(new ObjectPrivate)
{
}

ScriptEngine::Object::~Object()
{
    delete mPrivate;
}

std::shared_ptr<ScriptEngine::Object> ScriptEngine::Object::registerFunction(const String &name, Function &&func)
{
    std::shared_ptr<ScriptEngine::Object> obj = createObject(shared_from_this(), CustomType_Function, name);
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

std::shared_ptr<ScriptEngine::Object> ScriptEngine::Object::child(const String &name)
{
    auto ch = mPrivate->children.find(name);
    if (ch != mPrivate->children.end())
        return ch->second;

    return createObject(shared_from_this(), CustomType_Object, name);
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
                                 const std::shared_ptr<ScriptEngine::Object> &thisObject,
                                 String *error)
{
    assert(mPrivate->customType == CustomType_Function || mPrivate->customType == CustomType_AdoptedFunction);
    if (mPrivate->customType == CustomType_Function) {
        return mPrivate->func(arguments);
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

void ScriptEngine::Object::setExtraData(const Value &value)
{
    mPrivate->extraData = value;
}

const Value &ScriptEngine::Object::extraData() const
{
    return mPrivate->extraData;
}

std::shared_ptr<ScriptEngine::Object> ScriptEngine::toObject(const Value &value) const
{
    const std::shared_ptr<ScriptEngineCustom> &custom = std::static_pointer_cast<ScriptEngineCustom>(value.toCustom());
    if (!custom || custom->object.IsEmpty()) {
        return std::shared_ptr<ScriptEngine::Object>();
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
    return result;
}

static v8::Handle<v8::Value> toV8(v8::Isolate* isolate, const Value& value)
{
    v8::EscapableHandleScope handleScope(isolate);
    return handleScope.Escape(toV8_helper(isolate, value));
}
