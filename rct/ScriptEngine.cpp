#include "ScriptEngine.h"
#include <v8.h>
#include <rct/EventLoop.h>

ScriptEngine *ScriptEngine::sInstance = 0;
ScriptEngine::ScriptEngine()
{
    assert(!sInstance);
    sInstance = this;
}

ScriptEngine::~ScriptEngine()
{
    assert(sInstance == this);
    sInstance = 0;
    delete mGlobalObject;
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
