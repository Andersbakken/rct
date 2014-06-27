#ifndef ScriptEngine_h
#define ScriptEngine_h

#include <rct/String.h>
#include <rct/Value.h>
#include <rct/Hash.h>

class ObjectPrivate;
struct ScriptEnginePrivate;
class ScriptEngine
{
public:
    ScriptEngine();
    ~ScriptEngine();

    static ScriptEngine *instance() { return sInstance; }

    bool checkSyntax(const String &source, const Path &path = Path(), String *error = 0) const;
    Value evaluate(const String &source, const Path &path, String *error);

    void throwError(const String &error);

    class Object
    {
    public:
        ~Object();

        typedef std::function<Value (const List<Value> &)> Function;
        typedef std::function<Value ()> Getter;
        typedef std::function<void (const Value &)> Setter;

        void registerFunction(const String &name, Function &&func);
        void registerProperty(const String &name, Getter &&get);
        void registerProperty(const String &name, Getter &&get, Setter &&set);

        Object *child(const String &name);
    private:
        Object();

        ObjectPrivate *mPrivate;
        friend class ScriptEngine;
    };

    Object *globalObject() const { return mGlobalObject; }
private:
    static ScriptEngine *sInstance;
    ScriptEnginePrivate *mPrivate;
    Object *mGlobalObject;
};

#endif
