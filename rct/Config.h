#ifndef Config_h
#define Config_h

#include <getopt.h>
#include <stdio.h>
#include <rct/Path.h>
#include <rct/String.h>
#include <rct/Value.h>
#include <assert.h>
#include <string.h>
#include <functional>
#include <type_traits>

#include "rct/List.h"
#include "rct/String.h"


class Config
{
public:
    static bool parse(int argc, char **argv, const List<Path> &rcFiles = List<Path>());

    template<typename T, int listCount = 0>
    static void registerListOption(const char *name, const String &description, const char shortOpt = '\0',
                                   const List<T> &defaultValue = List<T>(),
                                   const std::function<bool(const List<T>&, String &)> &validator = std::function<bool(const List<T>&, String &)>())
    {
        const Value def = Value::create(defaultValue);
        const Value::Type type = Value::create(T()).type();
        ListOption<T> *option = new ListOption<T>;
        option->validator = validator;
        option->name = name;
        option->description = description;
        option->shortOption = shortOpt;
        option->defaultValue = defaultValue;
        option->type = type;
        option->count = 0;
        option->listCount = listCount;
        sOptions.append(option);
    }

    template <typename T>
    static void registerOption(const char *name,
                               const String &description,
                               const char shortOpt = '\0',
                               const T &defaultValue = T(),
                               const std::function<bool(const T&, String &)> &validator = std::function<bool(const T&, String &)>())
    {
        const Value def = Value::create(defaultValue);
        Option<T> *option = new Option<T>;
        option->validator = validator;
        option->name = name;
        option->description = description;
        option->shortOption = shortOpt;
        option->defaultValue = def;
        option->type = def.type();
        option->count = 0;
        option->listCount = 0;
        sOptions.append(option);
    }

    static int isEnabled(const char *name)
    {
        const OptionBase *opt = findOption(name);
        if (opt && opt->value.toBool()) {
            return opt->count;
        }
        return 0;
    }

    template <typename T> static T value(const char *name, const T & defaultValue, bool *ok = nullptr)
    {
        const OptionBase *opt = findOption(name);
        if (opt && !opt->value.isNull()) {
            if (ok)
                *ok = true;
            return opt->value.convert<T>();
        }
        if (ok)
            *ok = true;
        return defaultValue;
    }

    template <typename T> static T value(const char *name, bool *ok = nullptr)
    {
        const OptionBase *opt = findOption(name);
        if (opt) {
            if (opt->value.isNull()) {
                T ret;
                convert(opt->defaultValue, ret, ok);
                return ret;
            } else {
                T ret;
                convert(opt->value, ret, ok);
                return ret;
            }
        }
        if (ok)
            *ok = false;
        return T();
    }
    static void showHelp(FILE *f);
    static void setAllowsFreeArguments(bool on) { sAllowsFreeArgs = on; }
    static bool allowsFreeArguments() { return sAllowsFreeArgs; }
    static List<Value> freeArgs() { return sFreeArgs; }
    static void clear();
private:
    template <class T> struct is_list { static const int value = 0; };
    template <class T> struct is_list<List<T> > { static const int value = 1; };

    template <class T> class ListType {
    private:
        template <class U>
        struct ident
        {
            typedef U type;
        };

        template <class C>
        static ident<C> test(List<C>);

        static ident<void> test(...);

        typedef decltype(test(T())) list_type;
    public:
        typedef typename list_type::type type;
    };

    template <typename T>
    static void convert(const Value &value, T &t, bool *ok = nullptr, typename std::enable_if<is_list<T>::value, T>::type * = 0)
    {
        typedef typename ListType<T>::type K;
        List<Value> values = value.convert<List<Value> >();
        t.reserve(values.size());
        for (const Value &val : values) {
            bool o;
            const K k = val.convert<K>(&o);
            if (!o) {
                t.clear();
                if (ok)
                    *ok = false;
                return;
            }
            t.append(k);
        }
        if (ok)
            *ok = true;
    }
    template <typename T>
    static void convert(const Value &value, T &t, bool *ok = nullptr, typename std::enable_if<!is_list<T>::value, T>::type * = 0)
    {
        t = value.convert<T>(ok);
    }
    Config();
    ~Config();
    struct OptionBase {
        virtual ~OptionBase() {}
        const char *name;
        char shortOption;
        String description;
        Value defaultValue;
        Value value;
        Value::Type type;
        size_t count, listCount;
        virtual bool validate(String &err) = 0;
    };
    template <typename T>
    struct Option : public OptionBase {
        virtual bool validate(String &err) override
        {
            if (validator) {
                const T t = value.convert<T>();
                if (!validator(t, err)) {
                    value.clear();
                    return false;
                }
            }
            return true;
        }
        std::function<bool(const T &, String &err)> validator;
    };

    template <typename T>
    struct ListOption : public OptionBase {
        virtual bool validate(String &err) override
        {
            if (validator) {
                const List<Value> t = value.convert<List<Value> >();
                List<T> converted(t.size());
                for (int i=0; i<t.size(); ++i) {
                    converted[i] = t.at(i).convert<T>();
                }
                if (!validator(converted, err)) {
                    value.clear();
                    return false;
                }
            }
            return true;
        }
        std::function<bool(const List<T> &, String &err)> validator;
    };

    static List<OptionBase*> sOptions;
    static bool sAllowsFreeArgs;
    static List<Value> sFreeArgs;
    static const OptionBase *findOption(const char *name)
    {
        assert(name);
        const int len = strlen(name);
        for (size_t i=0; i<sOptions.size(); ++i) {
            if (!strcmp(sOptions.at(i)->name, name) || (len == 1 && *name == sOptions.at(i)->shortOption)) {
                return sOptions.at(i);
            }
        }
        return nullptr;
    }
};

#endif
