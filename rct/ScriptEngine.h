#ifndef ScriptEngine_h
#define ScriptEngine_h

#include <rct/rct-config.h>

#ifdef HAVE_SCRIPTENGINE
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

    static ScriptEngine *instance() { return sInstance; }

    Value evaluate(const String &source, const Path &path = String(), String *error = 0);
    Value call(const String &function, String *error = 0);
    Value call(const String &function, std::initializer_list<Value> arguments, String *error = 0);

    template<typename RetVal>
    RetVal throwException(const Value& exception) {
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
        typedef std::shared_ptr<Object> SharedPtr;
        typedef std::weak_ptr<Object> WeakPtr;

        ~Object();

        SharedPtr registerFunction(const String &name, Function &&func);
        void registerProperty(const String &name, Getter &&get);
        void registerProperty(const String &name, Getter &&get, Setter &&set);

        SharedPtr child(const String &name);
        bool isFunction() const;

        Value property(const String &propertyName, String *error = 0);
        void setProperty(const String &propertyName, const Value &value, String *error = 0);
        Value call(std::initializer_list<Value> arguments = std::initializer_list<Value>(),
                   const SharedPtr &thisObject = SharedPtr(),
                   String *error = 0);

        template<typename T>
        void setExtraData(const T& t, int type = -1);
        template<typename T>
        T extraData(int type = -1, bool *ok = 0) const;
        int extraDataType() const;

        Signal<std::function<void(const SharedPtr&)> >& onDestroyed() { return mDestroyed; }

        // callAsConstructor
        // handleUnknownProperty
        // deleteHandler
    private:
        Object();
        Object(const Object&) = delete;
        Object& operator=(const Object&) = delete;

        ObjectPrivate *mPrivate;
        friend class ScriptEngine;
        friend class ObjectPrivate;

        class ExtraDataBase
        {
        public:
            int type;
            ExtraDataBase(int t) : type(t) {}
            virtual ~ExtraDataBase() { };
        };

        template<typename T>
        class ExtraData : public ExtraDataBase
        {
        public:
            ExtraData(int type, const T& ot)
                : ExtraDataBase(type), t(new T(ot))
            {
            }
            ~ExtraData()
            {
                delete t;
            }
            T *t;
        };

        ExtraDataBase* mData;
        Signal<std::function<void(const SharedPtr&)> > mDestroyed;

        friend struct ObjectData;
    };

    class Class : public std::enable_shared_from_this<Class>
    {
    public:
        typedef std::shared_ptr<Class> SharedPtr;
        typedef std::weak_ptr<Class> WeakPtr;

        ~Class();

        enum QueryResult {
            None = 0x0,
            ReadOnly = 0x1,
            DontEnum = 0x2,
            DontDelete = 0x4
        };

        // return an empty Value from these to not intercept

        // return Value for both Set and Get
        typedef std::function<Value(const Object::SharedPtr&, const String&, const Value&)> InterceptSet;
        typedef std::function<Value(const Object::SharedPtr&, const String&)> InterceptGet;
        // return QueryResult for Query, boolean for Deleter
        typedef std::function<Value(const String&)> InterceptQuery;
        // return List<Value>
        typedef std::function<Value()> InterceptEnumerate;
        typedef std::function<Value(const List<Value>&)> Constructor;

        static SharedPtr create(const String& name)
        {
            SharedPtr cls(new Class(name));
            cls->init();
            return cls;
        }

        void registerFunction(const String &name, Function &&func);
        void registerStaticFunction(const String &name, StaticFunction &&func);
        void registerProperty(const String &name, Getter &&get);
        void registerProperty(const String &name, Getter &&get, Setter &&set);
        void registerConstructor(Constructor&& ctor);

        void interceptPropertyName(InterceptGet&& get,
                                   InterceptSet&& set,
                                   InterceptQuery&& query,
                                   InterceptQuery&& deleter,
                                   InterceptEnumerate&& enumerator);

        Object::SharedPtr create();

    private:
        Class(const String& name);
        Class(const Class&) = delete;
        Class& operator=(const Class&) = delete;

        void init();

        ClassPrivate *mPrivate;
        friend class ScriptEngine;
        friend class ClassPrivate;
        friend class ObjectPrivate;
    };

    Value fromObject(const Object::SharedPtr& object);
    Object::SharedPtr toObject(const Value &value) const;
    bool isFunction(const Value &value) const;
    Object::SharedPtr globalObject() const { return mGlobalObject; }
    Object::SharedPtr createObject() const;

private:
    void throwExceptionInternal(const Value &exception);
    static ScriptEngine *sInstance;
    ScriptEnginePrivate *mPrivate;
    Object::SharedPtr mGlobalObject;

    friend struct ScriptEnginePrivate;
};

template<typename T>
inline void ScriptEngine::Object::setExtraData(const T& t, int type)
{
    delete mData;
    mData = new ExtraData<T>(type, t);
}

template<typename T>
T ScriptEngine::Object::extraData(int type, bool *ok) const
{
    if (!mData || (type != -1 && mData->type != type)) {
        if (!ok)
            *ok = false;
        return T();
    } else if (ok) {
        *ok = false;
    }
    return *(static_cast<ExtraData<T>*>(mData)->t);
}

inline int ScriptEngine::Object::extraDataType() const
{
    return mData ? mData->type : -1;
}

template<>
inline Value ScriptEngine::throwException<Value>(const Value& exception) {
    throwExceptionInternal(exception);
    return Value::undefined();
}

#endif // HAVE_SCRIPTENGINE
#endif // ScriptEngine_h
