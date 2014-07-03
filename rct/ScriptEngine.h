#ifndef ScriptEngine_h
#define ScriptEngine_h

#include "rct-config.h"

#ifdef HAVE_SCRIPTENGINE
#include <rct/String.h>
#include <rct/Value.h>
#include <rct/Hash.h>
#include <memory>

class ObjectPrivate;
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

    class Object : public std::enable_shared_from_this<Object>
    {
    public:
        ~Object();

        typedef std::function<Value (const List<Value> &)> Function;
        typedef std::function<Value ()> Getter;
        typedef std::function<void (const Value &)> Setter;

        std::shared_ptr<Object> registerFunction(const String &name, Function &&func);
        void registerProperty(const String &name, Getter &&get);
        void registerProperty(const String &name, Getter &&get, Setter &&set);

        std::shared_ptr<Object> child(const String &name);
        bool isFunction() const;

        Value property(const String &propertyName, String *error = 0);
        void setProperty(const String &propertyName, const Value &value, String *error = 0);
        Value call(std::initializer_list<Value> arguments,
                   const std::shared_ptr<Object> &thisObject = std::shared_ptr<Object>(),
                   String *error = 0);

        void setExtraData(const Value &value);
        const Value &extraData() const;

        // callAsConstructor
        // handleUnknownProperty
        // deleteHandler
    private:
        Object();

        ObjectPrivate *mPrivate;
        friend class ScriptEngine;
        friend class ObjectPrivate;
    };

    std::shared_ptr<Object> toObject(const Value &value) const;
    std::shared_ptr<Object> globalObject() const { return mGlobalObject; }
private:
    void throwExceptionInternal(const Value &exception);
    static ScriptEngine *sInstance;
    ScriptEnginePrivate *mPrivate;
    std::shared_ptr<Object> mGlobalObject;

    friend struct ScriptEnginePrivate;
};
#endif // HAVE_SCRIPTENGINE
#endif // ScriptEngine_h
