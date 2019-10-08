#ifndef ScriptEngine_h
#define ScriptEngine_h

#include <rct/rct-config.h>

#ifdef HAVE_SCRIPTENGINE

#include <v8.h>
#include <memory>

#include <rct/String.h>
#include <rct/Value.h>
#include <rct/SignalSlot.h>


class ObjectPrivate;
class ClassPrivate;
struct ScriptEnginePrivate;
class ScriptEngine
{
public:
    ScriptEngine();
    ~ScriptEngine();

    enum PrivateDataType {
        Type_Function = -100,
        Type_GlobalObject = -99,
        Type_FirstUser = 0
    };
    struct PrivateData
    {
        PrivateData(int t)
            : type(t)
        {}
        virtual ~PrivateData()
        {}

        const int type;
    };

    static ScriptEngine *instance() { return sInstance; }

    Value evaluate(const String &source, const Path &path = String(), String *error = nullptr);
    Value call(const String &function, String *error = nullptr);
    Value call(const String &function, std::initializer_list<Value> arguments, String *error = nullptr);

    template<typename RetVal>
    RetVal throwException(const Value &exception)
    {
        throwExceptionInternal(exception);
        return RetVal();
    }

    class Object;

    typedef std::function<Value (const std::shared_ptr<Object>& obj, const List<Value> &)> Function;
    typedef std::function<Value (const List<Value> &)> StaticFunction;
    typedef std::function<Value (const std::shared_ptr<Object>& obj)> Getter;
    typedef std::function<void (const std::shared_ptr<Object>& obj, const Value &)> Setter;

    class Object : public std::enable_shared_from_this<Object>
    {
    public:
        ~Object();

        std::shared_ptr<Object> registerFunction(const String &name, Function &&func);
        void registerProperty(const String &name, Getter &&get);
        void registerProperty(const String &name, Getter &&get, Setter &&set);

        std::shared_ptr<Object> child(const String &name);
        bool isFunction() const;

        Value property(const String &propertyName, String *error = nullptr);
        void setProperty(const String &propertyName, const Value &value, String *error = nullptr);
        Value call(std::initializer_list<Value> arguments = std::initializer_list<Value>(),
                   const std::shared_ptr<Object> &thisObject = nullptr,
                   String *error = nullptr);

        const PrivateData *privateData() const;
        template <typename T>
        const T *privateData(int type) const
        {
            const PrivateData *d = privateData();
            if (d && d->type == type)
                return static_cast<T *>(d);
            return nullptr;
        }

        PrivateData *privateData();
        template <typename T>
        T *privateData(int type)
        {
            PrivateData *d = privateData();
            if (d && d->type == type)
                return static_cast<T *>(d);
            return nullptr;
        }

        Signal<std::function<void(const std::shared_ptr<Object> &)> >& onDestroyed() { return mDestroyed; }

        // callAsConstructor
        // handleUnknownProperty
        // deleteHandler
    private:
        Object(v8::Isolate *isolate, const v8::Local<v8::Object> &object);
        Object(const Object&) = delete;
        Object &operator=(const Object&) = delete;

        v8::Persistent<v8::Object> mObject;
        friend class ScriptEngine;
        friend class ObjectPrivate;

        Signal<std::function<void(const std::shared_ptr<Object>&)> > mDestroyed;

        friend struct ObjectData;
    };

    class Class : public std::enable_shared_from_this<Class>
    {
    public:
        ~Class();

        enum QueryResult {
            None = 0x0,
            ReadOnly = 0x1,
            DontEnum = 0x2,
            DontDelete = 0x4
        };

        // return an empty Value from these to not intercept

        // return Value for both Set and Get
        typedef std::function<Value(const std::shared_ptr<Object> &, const String&, const Value&)> InterceptSet;
        typedef std::function<Value(const std::shared_ptr<Object> &, const String&)> InterceptGet;
        typedef std::function<PrivateData *()> CreatePrivateData;
        // return QueryResult for Query, boolean for Deleter
        typedef std::function<Value(const String&)> InterceptQuery;
        // return List<Value>
        typedef std::function<Value()> InterceptEnumerate;
        typedef std::function<Value(const List<Value>&)> Constructor;

        static std::shared_ptr<Class> create(int type, const String &name)
        {
            std::shared_ptr<Class> cls(new Class(type, name));
            cls->init();
            return cls;
        }

        void registerFunction(const String &name, Function &&func);
        void registerStaticFunction(const String &name, StaticFunction &&func);
        void registerProperty(const String &name, Getter &&get, Setter &&set = nullptr);
        void registerConstructor(Constructor&& ctor);
        void registerCreatePrivateData(CreatePrivateData &&createPrivateData);

        void interceptProperties(InterceptGet&& get,
                                 InterceptSet&& set,
                                 InterceptQuery&& query,
                                 InterceptQuery&& deleter,
                                 InterceptEnumerate&& enumerator);

        std::shared_ptr<Object> create();

    private:
        Class(int type, const String &name);
        Class(const Class&) = delete;
        Class &operator=(const Class&) = delete;

        void init();


        struct Property {
            Getter getter;
            Setter setter;
        };
        const int mType;
        const String mName;
        Constructor mConstructor;
        CreatePrivateData mCreatePrivateData;
        InterceptGet mInterceptGet;
        InterceptSet mInterceptSet;
        InterceptQuery mInterceptQuery;
        InterceptQuery mInterceptDelete;
        std::unordered_map<std::string, Property> mGetterSetters;
        std::unordered_map<std::string, StaticFunction> mStaticFunctions;
        std::unordered_map<std::string, Function> mFunctions;

        friend class ScriptEngine;
    };

    Value fromObject(const std::shared_ptr<Object>& object);
    std::shared_ptr<Object> toObject(const Value &value) const;
    bool isFunction(const Value &value) const;
    std::shared_ptr<Object> globalObject() const { return mGlobalObject; }
    std::shared_ptr<Object> createObject() const;

private:
    void throwExceptionInternal(const Value &exception);
    static ScriptEngine *sInstance;
    std::shared_ptr<Object> mGlobalObject;
    v8::Persistent<v8::Context> mContext;
    v8::Isolate *mIsolate { nullptr };
    struct ArrayBufferAllocator : public v8::ArrayBuffer::Allocator
    {
        virtual void* Allocate(size_t length) { return calloc(length, 1); }
        virtual void* AllocateUninitialized(size_t length) { return malloc(length); }
        virtual void Free(void* data, size_t /*length*/) { free(data); }
    } mArrayBufferAllocator;
};

template<>
inline Value ScriptEngine::throwException<Value>(const Value &exception) {
    throwExceptionInternal(exception);
    return Value::undefined();
}

#endif // HAVE_SCRIPTENGINE
#endif // ScriptEngine_h
