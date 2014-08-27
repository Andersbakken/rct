#ifndef Config_h
#define Config_h

#include <stdio.h>
#include <rct/Path.h>
#include <rct/Rct.h>
#include <rct/Value.h>
#include <rct/String.h>
#include <getopt.h>

class Config
{
public:
    static void parse(int argc, char **argv, const List<Path> &rcFiles = List<Path>());

    template<typename T, int listCount = 0>
    static void registerListOption(const char *name, const String &description, const char shortOpt = '\0',
                                   const List<T> &defaultValue = List<T>())
    {
        const Value def = Value::create(defaultValue);
        const Value::Type type = Value::create(T()).type();
        const Option option = { name, shortOpt, description, def, Value(), type, 0, listCount };
        sOptions.append(option);
    }


    template <typename T>
    static void registerOption(const char *name, const String &description, const char shortOpt = '\0', const T &defaultValue = T())
    {
        const Value def = Value::create(defaultValue);
        const Option option = { name, shortOpt, description, def, Value(), def.type(), 0, 0 };
        sOptions.append(option);
    }

    static bool isEnabled(const char *name, int *count = 0) // ### should return an int count of enabled-ness
    {
        const Option *opt = findOption(name);
        if (count)
            *count = opt ? opt->count : 0;
        return opt && opt->value.toBool();
    }

    template <typename T> static T value(const char *name, const T & defaultValue, bool *ok = 0)
    {
        const Option *opt = findOption(name);
        if (opt && !opt->value.isNull()) {
            if (ok)
                *ok = true;
            return opt->value.convert<T>();
        }
        if (ok)
            *ok = true;
        return defaultValue;
    }

    template <typename T> static T value(const char *name, bool *ok = 0)
    {
        const Option *opt = findOption(name);
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
    static void convert(const Value &value, T &t, bool *ok = 0, typename std::enable_if<is_list<T>::value, T>::type * = 0)
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
    static void convert(const Value &value, T &t, bool *ok = 0, typename std::enable_if<!is_list<T>::value, T>::type * = 0)
    {
        t = value.convert<T>(ok);
    }
    Config();
    ~Config();
    struct Option {
        const char *name;
        char shortOption;
        String description;
        Value defaultValue;
        Value value;
        Value::Type type;
        int count, listCount;
    };
    static List<Option> sOptions;
    static bool sAllowsFreeArgs;
    static List<Value> sFreeArgs;
    static const Option *findOption(const char *name)
    {
        assert(name);
        const int len = strlen(name);
        for (int i=0; i<sOptions.size(); ++i) {
            if (!strcmp(sOptions.at(i).name, name) || (len == 1 && *name == sOptions.at(i).shortOption)) {
                return &sOptions.at(i);
            }
        }
        return 0;
    }
};

#endif
