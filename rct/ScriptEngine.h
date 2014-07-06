#ifndef ScriptEngine_h
#define ScriptEngine_h

#include "rct-config.h"

#ifdef HAVE_SCRIPTENGINE
#include <rct/Log.h>
#include <rct/String.h>
#include <rct/Value.h>
#include <rct/Hash.h>
#include <memory>

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

    template <typename RetVal = Value>
    RetVal throwException(const Value& exception) {
        throwExceptionInternal(exception);
        return RetVal();
    }

    typedef std::function<Value (const List<Value> &)> Function;
    typedef std::function<Value ()> Getter;
    typedef std::function<void (const Value &)> Setter;

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

        void setExtraData(const Value &value);
        const Value &extraData() const;

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
    };

    class Class : public std::enable_shared_from_this<Class>
    {
    public:
        typedef std::shared_ptr<Class> SharedPtr;
        typedef std::weak_ptr<Class> WeakPtr;

        ~Class();

        static SharedPtr create(const String& name) { return SharedPtr(new Class(name)); }

        void registerFunction(const String &name, Function &&func);
        void registerProperty(const String &name, Getter &&get);
        void registerProperty(const String &name, Getter &&get, Setter &&set);

        Object::SharedPtr create();

    private:
        Class(const String& name);
        Class(const Class&) = delete;
        Class& operator=(const Class&) = delete;

        ClassPrivate *mPrivate;
        friend class ScriptEngine;
        friend class ClassPrivate;
        friend class ObjectPrivate;
    };

    Value fromObject(const Object::SharedPtr& object);
    Object::SharedPtr toObject(const Value &value) const;
    Object::SharedPtr globalObject() const { return mGlobalObject; }
private:
    void throwExceptionInternal(const Value &exception);
    static ScriptEngine *sInstance;
    ScriptEnginePrivate *mPrivate;
    Object::SharedPtr mGlobalObject;

    friend struct ScriptEnginePrivate;
};

#endif // HAVE_SCRIPTENGINE
#endif // ScriptEngine_h
